#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QSet>
#include <QString>
#include <QDateTime>
#include <QFileInfo>

class OpenMVAutoWatcher : public QObject
{
    Q_OBJECT

public:
    static OpenMVAutoWatcher *instance();
    explicit OpenMVAutoWatcher(QObject *parent = nullptr);
    ~OpenMVAutoWatcher() override;

    void watchFile(const QString &filePath);
    void unwatchFile(const QString &filePath);
    void setAutoReloadEnabled(bool enabled);
    bool isAutoReloadEnabled() const { return m_enabled; }
    void setDebounceInterval(int milliseconds);

signals:
    void fileModifiedExternally(const QString &filePath);

private slots:
    void onFileChanged(const QString &path);
    void onDebounceTimeout();

private:
    static OpenMVAutoWatcher *s_instance;
    QFileSystemWatcher m_watcher;
    QTimer m_debounceTimer;
    QSet<QString> m_pendingReloadFiles;
    QMap<QString, QDateTime> m_lastReloadTimes;
    bool m_enabled = true;
    int m_debounceMs = 300;
};
