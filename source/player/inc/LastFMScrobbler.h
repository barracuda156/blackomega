//-------------------------------------------------------------------------------------------
#ifndef __OMEGA_PLAYER_LASTFMSCROBBLER_H
#define __OMEGA_PLAYER_LASTFMSCROBBLER_H
//-------------------------------------------------------------------------------------------

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTimer>
#include <QSettings>
#include <QQueue>

#include "track/info/inc/Info.h"

//-------------------------------------------------------------------------------------------

// Forward declarations for liblastfm
namespace lastfm
{
    class Audioscrobbler;
    class MutableTrack;
}

//-------------------------------------------------------------------------------------------
namespace omega
{
namespace player
{
//-------------------------------------------------------------------------------------------

class LastFMScrobbler : public QObject
{
    public:
        Q_OBJECT

    public:
        LastFMScrobbler(QObject *parent = 0);
        virtual ~LastFMScrobbler();

        // Configuration
        bool isEnabled() const;
        void setEnabled(bool enabled);

        QString username() const;
        void setUsername(const QString& username);

        QString sessionKey() const;
        void setSessionKey(const QString& sessionKey);

        QString apiKey() const;
        void setApiKey(const QString& apiKey);

        QString apiSecret() const;
        void setApiSecret(const QString& apiSecret);

        bool isAuthenticated() const;

        // Authentication
        void authenticate(const QString& username, const QString& password);
        void logout();

        // Scrobbling
        void updateNowPlaying(QSharedPointer<track::info::Info> trackInfo);
        void checkScrobblePoint(quint64 currentTime, QSharedPointer<track::info::Info> trackInfo);
        void scrobble(QSharedPointer<track::info::Info> trackInfo, const QDateTime& timestamp);

        // Playback state
        void onTrackStarted(QSharedPointer<track::info::Info> trackInfo);
        void onTrackStopped();
        void onTrackPaused();
        void onTrackResumed();

        // Settings persistence
        void loadSettings();
        void saveSettings();

    Q_SIGNALS:
        void authenticationSucceeded();
        void authenticationFailed(const QString& error);
        void scrobbleSucceeded();
        void scrobbleFailed(const QString& error);
        void statusChanged(const QString& status);

    protected Q_SLOTS:
        void onAuthenticationFinished();
        void onScrobblesCached(const QList<lastfm::MutableTrack>& tracks);
        void onScrobblesSubmitted(const QList<lastfm::MutableTrack>& tracks);
        void onNowPlayingError(int error, const QString& message);
        void onScrobbleError(int error, const QString& message);

    protected:
        struct ScrobbleInfo
        {
            QSharedPointer<track::info::Info> trackInfo;
            QDateTime timestamp;
            quint64 trackStartTime;
            quint64 trackDuration;
            bool scrobbled;
            bool nowPlayingSent;
        };

        bool m_enabled;
        QString m_username;
        QString m_sessionKey;
        QString m_apiKey;
        QString m_apiSecret;
        bool m_authenticated;

        // QByteArray storage to keep API credentials alive for liblastfm
        QByteArray m_apiKeyUtf8;
        QByteArray m_apiSecretUtf8;
        QByteArray m_sessionKeyUtf8;
        QByteArray m_usernameUtf8;

        lastfm::Audioscrobbler *m_scrobbler;

        ScrobbleInfo m_currentTrack;
        QQueue<ScrobbleInfo> m_offlineQueue;

        static const int SCROBBLE_MIN_DURATION = 30; // seconds
        static const int SCROBBLE_THRESHOLD_TIME = 240; // 4 minutes in seconds
        static const float SCROBBLE_THRESHOLD_PERCENT; // 0.5 (50%)

        void initializeScrobbler();
        void shutdownScrobbler();

        bool shouldScrobble(const ScrobbleInfo& info, quint64 currentTime) const;
        lastfm::MutableTrack createLastFMTrack(QSharedPointer<track::info::Info> trackInfo) const;

        void processOfflineQueue();
        void saveOfflineQueue();
        void loadOfflineQueue();
};

//-------------------------------------------------------------------------------------------
} // namespace player
} // namespace omega
//-------------------------------------------------------------------------------------------
#endif
//-------------------------------------------------------------------------------------------
