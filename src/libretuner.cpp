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

#include "libretuner.h"
#include "logger.h"
#include "timerrunloop.h"
#include "ui/styledwindow.h"

#include "lt/libretuner.h"

#include <QDir>
#include <QDirIterator>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTextStream>

#include <fstream>
#include <future>
#include <memory>

#include "uiutil.h"

static LibreTuner * _global;

namespace fs = std::filesystem;

LibreTuner::LibreTuner(int & argc, char * argv[])
    : QApplication(argc, argv), rootPath_(fs::current_path())
{
    _global = this;

    Logger::debug("Initializing resources");

    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(definitions);
    Q_INIT_RESOURCE(style);
    Q_INIT_RESOURCE(codes);

    setOrganizationDomain("libretuner.org");
    setApplicationName("LibreTuner");

    // Setup LT context
    lt::setLogCallback(
        [](const std::string & message) { Logger::debug(message); });

    // intolib rewrite

    // Setup main path
    rootPath_ = fs::current_path();

    Logger::debug("Loading platforms");

    fs::path definitionPath = rootPath_ / "definitions";
    if (!fs::exists(definitionPath))
    {
        // Copy definitions
        Logger::info("Copying definitions to " + definitionPath.string());
        QDir destDir(QString::fromStdString(definitionPath.string()));
        destDir.mkpath(".");

        QDir sourceDir(":/definitions");
        QDirIterator it(sourceDir, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            QString path = it.next();
            QString relativePath = sourceDir.relativeFilePath(path);
            if (it.fileInfo().isDir())
            {
                destDir.mkpath(relativePath);
                continue;
            }

            if (!it.fileInfo().isFile())
                continue;

            QFile file(path);
            QString toPath(destDir.filePath(relativePath));
            Logger::debug("Copying " + toPath.toStdString());
            if (!file.copy(toPath))
                Logger::warning("Failed to copy definition: " + toPath.toStdString());
        }
    }

    catchCritical(
        [&]() { platforms_.loadDirectory(definitionPath); },
        "Error loading definitions");

    links_.setPath(rootPath_ / "links.lts");

    // Load links
    catchCritical([this]() { load_datalinks(); },
                  tr("Error loading datalinks"));

    // Load DTC descriptions
    dtcDescriptions_.load();

    currentDatalink_ = links_.getFirst();
    currentPlatform_ = platforms_.first();

    setWindowIcon(QIcon(":/icons/libretuner_transparent.png"));

#ifdef WIN32
    {
        QFile f(":qdarkstyle/style.qss");
        if (f.exists())
        {
            f.open(QFile::ReadOnly | QFile::Text);
            QTextStream ts(&f);
            setStyleSheet(ts.readAll());
        }
    }
#endif

    TimerRunLoop::get().startWorker();
    mainWindow_ = new MainWindow;
    mainWindow_->show();
}

LibreTuner * LibreTuner::get() { return _global; }

void LibreTuner::load_datalinks()
{
    Logger::debug("Loading datalinks");
    links_.detect();

    try
    {
        links_.load();
    }
    catch (const std::runtime_error & err)
    {
        QMessageBox::warning(nullptr, tr("Datalink database error"),
                             tr("Failed to load datalink save data: ") +
                                 err.what());
    }
}

LibreTuner::~LibreTuner() { _global = nullptr; }

void LibreTuner::setup()
{
    // SetupDialog setup;
    // setup.setDefinitionModel(DefinitionManager::get());
    // DataLinksListModel model;
    // setup.setDatalinksModel(&model);
    // setup.exec();
    // currentDefinition_ = setup.platform();
    // currentDatalink_ = setup.datalink();
}

void LibreTuner::saveLinks()
{
    try
    {
        links_.save();
    }
    catch (const std::runtime_error & err)
    {
        QMessageBox::critical(nullptr, tr("Datalink save error"),
                              tr("Failed to save datalink database: ") +
                                  err.what());
    }
}

void LibreTuner::setPlatform(lt::PlatformPtr platform)
{
    currentPlatform_ = std::move(platform);

    if (platform)
        Logger::debug("Set platform to " + platform->name);
    else
        Logger::debug("Unset platform");
}

void LibreTuner::setDatalink(lt::DataLink * link)
{
    currentDatalink_ = link;

    if (link != nullptr)
        Logger::debug("Set datalink to " + link->name());
    else
        Logger::debug("Unset datalink");
}

lt::PlatformLink LibreTuner::platformLink() const
{
    if (currentDatalink_ == nullptr)
        throw std::runtime_error("no datalink is selected");
    if (!currentPlatform_)
        throw std::runtime_error("no platform is selected");

    return lt::PlatformLink(*currentDatalink_, *currentPlatform_);
}

lt::ProjectPtr LibreTuner::openProject(const std::filesystem::path & path)
{
    // Check if the project is already open
    if (QModelIndex index =
            projects_.projectIndex(QString::fromStdString(path.string()));
        index.isValid())
    {
        return index.data(Qt::UserRole).value<lt::ProjectPtr>();
    }

    auto project = std::make_shared<lt::Project>(path, platforms_);
    bool success = false;
    catchCritical(
        [&]() {
            project->load();
            success = true;
        },
        tr("Error loading project"));
    if (!success)
        return lt::ProjectPtr();

    projects_.addProject(project);
    return project;
}

lt::ProjectPtr LibreTuner::createProject(const std::filesystem::path & path,
                                         const std::string & name)
{
    auto project = std::make_shared<lt::Project>(path, platforms_);
    project->makeDirectories();
    project->setName(name);
    project->save();

    projects_.addProject(project);
    return project;
}
