#include "openmvautowatcher.h"
#include <QDebug>

OpenMVAutoWatcher *OpenMVAutoWatcher::s_instance = nullptr;

OpenMVAutoWatcher *OpenMVAutoWatcher::instance()
{
    if (!s_instance) {
        s_instance = new OpenMVAutoWatcher();
    }
    return s_instance;
}

OpenMVAutoWatcher::OpenMVAutoWatcher(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(m_debounceMs);

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &OpenMVAutoWatcher::onFileChanged);
    connect(&m_debounceTimer, &QTimer::timeout, this, &OpenMVAutoWatcher::onDebounceTimeout);
}

OpenMVAutoWatcher::~OpenMVAutoWatcher()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void OpenMVAutoWatcher::watchFile(const QString &filePath)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) return;

    if (!m_watcher.files().contains(filePath)) {
        m_watcher.addPath(filePath);
        qDebug() << "[OpenMV AutoWatcher] Watching file:" << filePath;
    }
}

void OpenMVAutoWatcher::unwatchFile(const QString &filePath)
{
    if (m_watcher.files().contains(filePath)) {
        m_watcher.removePath(filePath);
        m_pendingReloadFiles.remove(filePath);
    }
}

void OpenMVAutoWatcher::setAutoReloadEnabled(bool enabled)
{
    m_enabled = enabled;
}

void OpenMVAutoWatcher::setDebounceInterval(int milliseconds)
{
    m_debounceMs = qMax(100, milliseconds);
    m_debounceTimer.setInterval(m_debounceMs);
}

void OpenMVAutoWatcher::onFileChanged(const QString &path)
{
    if (!m_enabled) return;

    // Some editors delete and recreate files on save. If the path was removed, re-add it.
    if (QFileInfo::exists(path) && !m_watcher.files().contains(path)) {
        m_watcher.addPath(path);
    }

    m_pendingReloadFiles.insert(path);
    m_debounceTimer.start(); // restart debounce timer
}

void OpenMVAutoWatcher::onDebounceTimeout()
{
    if (!m_enabled) {
        m_pendingReloadFiles.clear();
        return;
    }

    for (const QString &file : m_pendingReloadFiles) {
        if (!QFileInfo::exists(file)) continue;

        QDateTime lastModified = QFileInfo(file).lastModified();
        if (m_lastReloadTimes.contains(file) && m_lastReloadTimes[file] == lastModified) {
            continue; // already processed this timestamp
        }

        m_lastReloadTimes[file] = lastModified;
        qDebug() << "[OpenMV AutoWatcher] Debounce trigger for file:" << file;
        emit fileModifiedExternally(file);
    }

    m_pendingReloadFiles.clear();
}
