/*
 * LibreTuner
 * Copyright (C) 2018 Altenius
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "rommanager.h"
#include "definitions/definition.h"
#include "libretuner.h"

#include <QFileInfo>
#include <cassert>

#include <iostream>
#include <fstream>

RomManager *RomManager::get() {
  static RomManager romManager;
  return &romManager;
}

bool RomManager::load() {
  LibreTuner::get()->checkHome();

  QString listPath = LibreTuner::get()->home() + "/" + "roms.xml";

  if (!QFile::exists(listPath)) {
    return true;
  }

  QFile listFile(listPath);
  if (!listFile.open(QFile::ReadOnly)) {
    lastError_ = listFile.errorString();
    return false;
  }

  QXmlStreamReader xml(&listFile);
  // set nextId_ to -1 for checking the highest ROM id
  nextId_ = -1;

  if (xml.readNextStartElement()) {
    if (xml.name() == "roms") {
      readRoms(xml);
    } else {
      xml.raiseError(QObject::tr("Unexpected element"));
    }
  }

  nextId_++;

  if (xml.error()) {
    lastError_ = QObject::tr("%1\nLine %2, column %3")
                     .arg(xml.errorString())
                     .arg(xml.lineNumber())
                     .arg(xml.columnNumber());
    return false;
  }

  emit updateRoms();

  return true;
}

void RomManager::readRoms(QXmlStreamReader &xml) {
  assert(xml.isStartElement() && xml.name() == "roms");
  roms_.clear();

  while (xml.readNextStartElement()) {
    if (xml.name() != "rom") {
      xml.raiseError("Unexpected element in roms");
      return;
    }

    RomPtr rom = std::make_shared<Rom>();
    rom->setId(-1);

    // Read ROM data
    while (xml.readNextStartElement()) {
      if (xml.name() == "name") {
        rom->setName(xml.readElementText().trimmed().toStdString());
      } else if (xml.name() == "path") {
        rom->setPath(xml.readElementText().trimmed().toStdString());
      } else if (xml.name() == "type") {
        QString type = xml.readElementText().toLower();
        DefinitionPtr def =
            DefinitionManager::get()->getDefinition(type.toStdString());
        if (!def) {
          xml.raiseError("Invalid ROM type");
          break;
        }
        rom->setDefinition(def->id());
      } else if (xml.name() == "subtype") {
        QString type = xml.readElementText().toLower();
        rom->setSubDefinition(type.toStdString());
        // TODO: check if this subtype exists
      } else if (xml.name() == "id") {
        bool ok;
        rom->setId(xml.readElementText().toInt(&ok));
        if (!ok) {
          xml.raiseError("id is not a valid decimal number");
        }
        if (rom->id() > nextId_) {
          nextId_ = rom->id();
        }
      }
    }

    // Verifications
    if (!xml.hasError()) {
      if (rom->name().empty()) {
        xml.raiseError("ROM name is empty");
      }
      if (rom->path().empty()) {
        xml.raiseError("ROM path is empty");
      }
      if (rom->definitionId().empty()) {
        xml.raiseError("ROM type is empty");
      }
      if (rom->subDefinitionId().empty()) {
        xml.raiseError("ROM subtype is empty");
      }
      if (rom->id() < 0) {
        xml.raiseError("ROM id is empty or negative");
      }
    }

    if (xml.hasError()) {
      return;
    }

    roms_.push_back(rom);
  }
}

bool RomManager::save() {
  LibreTuner::get()->checkHome();

  QString listPath = LibreTuner::get()->home() + "/" + "roms.xml";

  QFile listFile(listPath);
  if (!listFile.open(QFile::WriteOnly)) {
    lastError_ = listFile.errorString();
    return false;
  }

  QXmlStreamWriter xml(&listFile);
  xml.setAutoFormatting(true);
  xml.setAutoFormattingIndent(-1); // tabs > spaces

  xml.writeStartDocument();
  xml.writeDTD("<!DOCTYPE roms>");
  xml.writeStartElement("roms");
  for (const RomPtr &rom : roms_) {
    xml.writeStartElement("rom");
    xml.writeTextElement("name", QString::fromStdString(rom->name()));
    xml.writeTextElement("path", QString::fromStdString(rom->path()));
    xml.writeTextElement("id", QString::number(rom->id()));
    xml.writeTextElement("type", QString::fromStdString(rom->definitionId()));
    xml.writeTextElement("subtype",
                         QString::fromStdString(rom->subDefinitionId()));
    xml.writeEndElement();
  }
  xml.writeEndElement();
  xml.writeEndDocument();
  return true;
}

bool RomManager::addRom(const std::string &name, const DefinitionPtr &definition,
                        gsl::span<const uint8_t> data) {
  LibreTuner::get()->checkHome();

  QString romRoot = LibreTuner::get()->home() + "/roms/";
  QString path = QString::fromStdString(name);
  if (QFile::exists(path)) {
    int count = 0;
    do {
      path = QString::fromStdString(name) + QString::number(++count);
    } while (QFile::exists(romRoot + path));
  }

  std::ofstream file((romRoot + path).toStdString(), std::fstream::out | std::fstream::binary);
  file.write(reinterpret_cast<const char*>(data.data()), data.size());
  file.close();

  // Determine the subtype
  SubDefinitionPtr subtype = definition->identifySubtype(data);
  if (!subtype) {
    lastError_ = "Unknown firmware version or this is the wrong vehicle. If "
                 "this is the correct vehicle, please submit a bug report so "
                 "we can add support for this firmware version.";
    return false;
  }

  RomPtr rom = std::make_shared<Rom>();
  rom->setName(name);
  rom->setPath(path.toStdString());
  rom->setDefinition(definition->id());
  rom->setSubDefinition(subtype->id());
  rom->setId(nextId_++);
  roms_.push_back(rom);

  emit updateRoms();

  return save();
}

RomPtr RomManager::fromId(int id) {
  for (RomPtr &rom : roms_) {
    if (rom->id() == id) {
      return rom;
    }
  }

  return nullptr;
}