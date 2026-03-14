#include "player/inc/LastFMScrobbler.h"

// Define Qt keywords for liblastfm headers which use signals/slots/emit
// even when QT_NO_KEYWORDS is defined in the build
#ifndef signals
#define signals Q_SIGNALS
#endif
#ifndef slots
#define slots Q_SLOTS
#endif
#ifndef emit
#define emit Q_EMIT
#endif

#include <lastfm/Audioscrobbler.h>
#include <lastfm/Track.h>
#include <lastfm/ws.h>
#include <lastfm/XmlQuery.h>

#include <QNetworkReply>
#include <QDomDocument>

#include "common/inc/Log.h"

//-------------------------------------------------------------------------------------------
namespace omega
{
namespace player
{
//-------------------------------------------------------------------------------------------

const float LastFMScrobbler::SCROBBLE_THRESHOLD_PERCENT = 0.5f;

//-------------------------------------------------------------------------------------------

LastFMScrobbler::LastFMScrobbler(QObject *parent) : QObject(parent),
    m_enabled(false),
    m_username(),
    m_sessionKey(),
    m_apiKey(),
    m_apiSecret(),
    m_authenticated(false),
    m_scrobbler(0),
    m_currentTrack(),
    m_offlineQueue()
{
    loadSettings();

    // Embed Last.fm API credentials for BlackOmega
    // These identify the application to Last.fm's scrobbling service
    if(m_apiKey.isEmpty())
    {
        m_apiKey = "62ab61b59be59ce621c27bfeac13ae45";
    }

    if(m_apiSecret.isEmpty())
    {
        m_apiSecret = "ac988adf0943e72d00d9ef983edd119e";
    }

    /* OLD CODE - Load API keys from environment (kept for reference/debugging)
    // Load API key from environment if not set in settings
    if(m_apiKey.isEmpty())
    {
        QByteArray envKey = qgetenv("LASTFM_API_KEY");
        if(!envKey.isEmpty())
        {
            m_apiKey = QString::fromUtf8(envKey);
        }
    }

    // Load API secret from environment if not set in settings
    if(m_apiSecret.isEmpty())
    {
        QByteArray envSecret = qgetenv("LASTFM_API_SECRET");
        if(!envSecret.isEmpty())
        {
            m_apiSecret = QString::fromUtf8(envSecret);
        }
    }
    */

    // Don't auto-initialize scrobbler on startup to avoid crashes if liblastfm has issues
    // It will be initialized on first scrobble attempt or when updateNowPlaying is called
    if(m_enabled && !m_sessionKey.isEmpty() && !m_username.isEmpty())
    {
        // Mark as authenticated based on saved session, but delay scrobbler creation
        m_authenticated = true;
        Q_EMIT statusChanged("Ready to scrobble as " + m_username);
    }
}

//-------------------------------------------------------------------------------------------

LastFMScrobbler::~LastFMScrobbler()
{
    saveSettings();
    shutdownScrobbler();
}

//-------------------------------------------------------------------------------------------

bool LastFMScrobbler::isEnabled() const
{
    return m_enabled;
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::setEnabled(bool enabled)
{
    if(m_enabled != enabled)
    {
        m_enabled = enabled;

        if(m_enabled && !m_sessionKey.isEmpty() && !m_apiKey.isEmpty() && !m_apiSecret.isEmpty())
        {
            initializeScrobbler();
        }
        else if(!m_enabled)
        {
            shutdownScrobbler();
        }

        saveSettings();
    }
}

//-------------------------------------------------------------------------------------------

QString LastFMScrobbler::username() const
{
    return m_username;
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::setUsername(const QString& username)
{
    m_username = username;
    saveSettings();
}

//-------------------------------------------------------------------------------------------

QString LastFMScrobbler::sessionKey() const
{
    return m_sessionKey;
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::setSessionKey(const QString& sessionKey)
{
    m_sessionKey = sessionKey;
    saveSettings();
}

//-------------------------------------------------------------------------------------------

QString LastFMScrobbler::apiKey() const
{
    return m_apiKey;
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::setApiKey(const QString& apiKey)
{
    m_apiKey = apiKey;
    saveSettings();
}

//-------------------------------------------------------------------------------------------

QString LastFMScrobbler::apiSecret() const
{
    return m_apiSecret;
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::setApiSecret(const QString& apiSecret)
{
    m_apiSecret = apiSecret;
    saveSettings();
}

//-------------------------------------------------------------------------------------------

bool LastFMScrobbler::isAuthenticated() const
{
    return m_authenticated;
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::authenticate(const QString& username, const QString& password)
{
    if(m_apiKey.isEmpty() || m_apiSecret.isEmpty())
    {
        Q_EMIT authenticationFailed("API key and secret must be configured");
        Q_EMIT statusChanged("Error: API key and secret not configured");
        return;
    }

    m_username = username;

    // Configure Last.fm web services with API key and secret
    // Store as QByteArray to keep data alive for the const char* pointers
    m_apiKeyUtf8 = m_apiKey.toUtf8();
    m_apiSecretUtf8 = m_apiSecret.toUtf8();

    lastfm::ws::ApiKey = m_apiKeyUtf8.constData();
    lastfm::ws::SharedSecret = m_apiSecretUtf8.constData();

    // Use Last.fm web services to authenticate
    QMap<QString, QString> params;
    params["method"] = "auth.getMobileSession";
    params["username"] = username;
    params["password"] = password;

    QNetworkReply* reply = lastfm::ws::post(params);

    connect(reply, SIGNAL(finished()), this, SLOT(onAuthenticationFinished()));
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::onAuthenticationFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if(!reply)
    {
        return;
    }

    try
    {
        QByteArray data = reply->readAll();
        QDomDocument doc;
        doc.setContent(data);
        lastfm::XmlQuery lfm(doc.documentElement());

        if(lfm.children("error").size() > 0)
        {
            QString error = lfm["error"].text();
            m_authenticated = false;
            Q_EMIT authenticationFailed(error);
            Q_EMIT statusChanged("Authentication failed: " + error);
        }
        else
        {
            m_sessionKey = lfm["session"]["key"].text();
            m_username = lfm["session"]["name"].text();
            m_authenticated = true;

            initializeScrobbler();

            Q_EMIT authenticationSucceeded();
            Q_EMIT statusChanged("Authenticated as " + m_username);

            saveSettings();
        }
    }
    catch(std::exception& e)
    {
        m_authenticated = false;
        QString error = QString::fromUtf8(e.what());
        Q_EMIT authenticationFailed(error);
        Q_EMIT statusChanged("Authentication error: " + error);
    }

    reply->deleteLater();
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::logout()
{
    m_sessionKey.clear();
    m_authenticated = false;
    shutdownScrobbler();
    saveSettings();

    Q_EMIT statusChanged("Logged out");
}

//-------------------------------------------------------------------------------------------

// Temporary message handler to prevent qFatal from aborting during scrobbler initialization
static bool g_lastfmInitFailed = false;
static void lastfmInitMessageHandler(QtMsgType type, const char* msg)
{
    if(type == QtFatalMsg)
    {
        // liblastfm may call qFatal if it can't create cache directories
        // Log the error but don't abort the application
        common::Log::g_Log.print("LastFMScrobbler init error (non-fatal): ");
        common::Log::g_Log.print(msg);
        common::Log::g_Log.print("\n");
        g_lastfmInitFailed = true;
    }
    else
    {
        // For non-fatal messages, use default stderr output
        fprintf(stderr, "%s\n", msg);
    }
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::initializeScrobbler()
{
    if(m_scrobbler)
    {
        return;
    }

    if(m_sessionKey.isEmpty() || m_apiKey.isEmpty() || m_apiSecret.isEmpty())
    {
        return;
    }

    // Configure Last.fm web services
    // Store as QByteArray to keep data alive for the const char* pointers
    m_apiKeyUtf8 = m_apiKey.toUtf8();
    m_apiSecretUtf8 = m_apiSecret.toUtf8();
    m_sessionKeyUtf8 = m_sessionKey.toUtf8();
    m_usernameUtf8 = m_username.toUtf8();

    lastfm::ws::ApiKey = m_apiKeyUtf8.constData();
    lastfm::ws::SharedSecret = m_apiSecretUtf8.constData();
    lastfm::ws::SessionKey = m_sessionKeyUtf8.constData();
    lastfm::ws::Username = m_usernameUtf8.constData();

    // Note: We create the scrobbler but don't connect signals since Audioscrobbler
    // may not inherit from QObject in all liblastfm versions
    // Pass QString directly, not const char*, as Audioscrobbler uses it for cache identification

    // Install a custom message handler to prevent qFatal from aborting during scrobbler init
    // liblastfm may call qFatal if it can't create cache directories
    g_lastfmInitFailed = false;
    QtMsgHandler oldHandler = qInstallMsgHandler(lastfmInitMessageHandler);

    m_scrobbler = new lastfm::Audioscrobbler(m_apiKey);

    // Restore original message handler
    qInstallMsgHandler(oldHandler);

    if(g_lastfmInitFailed || m_scrobbler == 0)
    {
        if(m_scrobbler)
        {
            delete m_scrobbler;
            m_scrobbler = 0;
        }
        m_authenticated = false;
        common::Log::g_Log.print("LastFMScrobbler::initializeScrobbler - Scrobbler initialization failed, disabling\n");
    }
    else
    {
        m_authenticated = true;
        // Process any cached offline scrobbles
        processOfflineQueue();
    }
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::shutdownScrobbler()
{
    if(m_scrobbler)
    {
        delete m_scrobbler;
        m_scrobbler = 0;
    }
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::updateNowPlaying(QSharedPointer<track::info::Info> trackInfo)
{
    if(!m_enabled || !trackInfo)
    {
        return;
    }

    // Lazy initialization - create scrobbler on first use
    if(!m_scrobbler && m_authenticated)
    {
        initializeScrobbler();
    }

    if(!m_scrobbler)
    {
        return;
    }

    lastfm::MutableTrack track = createLastFMTrack(trackInfo);

    // Submit now playing using web services API directly
    try
    {
        track.stamp();  // This posts the now playing update
        m_currentTrack.nowPlayingSent = true;
    }
    catch(...)
    {
        // Silently ignore now playing errors
    }
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::checkScrobblePoint(quint64 currentTime, QSharedPointer<track::info::Info> trackInfo)
{
    if(!m_enabled || !trackInfo)
    {
        return;
    }

    // Lazy initialization - create scrobbler on first use
    if(!m_scrobbler && m_authenticated)
    {
        initializeScrobbler();
    }

    if(!m_scrobbler)
    {
        return;
    }

    if(m_currentTrack.scrobbled || !m_currentTrack.trackInfo)
    {
        return;
    }

    if(shouldScrobble(m_currentTrack, currentTime))
    {
        scrobble(trackInfo, QDateTime::currentDateTime());
        m_currentTrack.scrobbled = true;
    }
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::scrobble(QSharedPointer<track::info::Info> trackInfo, const QDateTime& timestamp)
{
    if(!m_enabled || !trackInfo)
    {
        return;
    }

    lastfm::MutableTrack track = createLastFMTrack(trackInfo);
    track.setTimeStamp(timestamp);

    if(m_scrobbler)
    {
        try
        {
            // Cache and submit the track
            m_scrobbler->cache(track);
            m_scrobbler->submit();
            Q_EMIT scrobbleSucceeded();
        }
        catch(...)
        {
            Q_EMIT scrobbleFailed("Failed to submit scrobble");
        }
    }
    else
    {
        // Queue for later submission when offline
        ScrobbleInfo info;
        info.trackInfo = trackInfo;
        info.timestamp = timestamp;
        info.scrobbled = false;
        info.nowPlayingSent = false;
        m_offlineQueue.enqueue(info);
        saveOfflineQueue();
    }
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::onTrackStarted(QSharedPointer<track::info::Info> trackInfo)
{
    if(!m_enabled || !trackInfo)
    {
        return;
    }

    m_currentTrack.trackInfo = trackInfo;
    m_currentTrack.timestamp = QDateTime::currentDateTime();
    m_currentTrack.trackStartTime = 0;
    m_currentTrack.trackDuration = static_cast<quint64>(trackInfo->length().secondsTotal());
    m_currentTrack.scrobbled = false;
    m_currentTrack.nowPlayingSent = false;

    updateNowPlaying(trackInfo);
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::onTrackStopped()
{
    m_currentTrack.trackInfo.clear();
    m_currentTrack.scrobbled = false;
    m_currentTrack.nowPlayingSent = false;
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::onTrackPaused()
{
    // Nothing special needed for pause
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::onTrackResumed()
{
    // Nothing special needed for resume
}

//-------------------------------------------------------------------------------------------

bool LastFMScrobbler::shouldScrobble(const ScrobbleInfo& info, quint64 currentTime) const
{
    if(!info.trackInfo)
    {
        return false;
    }

    // Track must be at least 30 seconds long
    if(info.trackDuration < SCROBBLE_MIN_DURATION)
    {
        return false;
    }

    // Calculate elapsed time
    quint64 elapsed = currentTime;

    // Scrobble when track has been played for half its duration or 4 minutes (whichever comes first)
    quint64 scrobbleThreshold = static_cast<quint64>(info.trackDuration * SCROBBLE_THRESHOLD_PERCENT);
    if(scrobbleThreshold > SCROBBLE_THRESHOLD_TIME)
    {
        scrobbleThreshold = SCROBBLE_THRESHOLD_TIME;
    }

    return elapsed >= scrobbleThreshold;
}

//-------------------------------------------------------------------------------------------

lastfm::MutableTrack LastFMScrobbler::createLastFMTrack(QSharedPointer<track::info::Info> trackInfo) const
{
    lastfm::MutableTrack track;

    if(!trackInfo)
    {
        return track;
    }

    track.setTitle(trackInfo->title());
    track.setArtist(trackInfo->artist());
    track.setAlbum(trackInfo->album());

    if(!trackInfo->track().isEmpty())
    {
        track.setTrackNumber(trackInfo->track().toInt());
    }

    track.setDuration(static_cast<int>(trackInfo->length().secondsTotal()));

    return track;
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::onScrobblesCached(const QList<lastfm::MutableTrack>& tracks)
{
    Q_EMIT statusChanged(QString("Cached %1 scrobble(s)").arg(tracks.size()));
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::onScrobblesSubmitted(const QList<lastfm::MutableTrack>& tracks)
{
    Q_EMIT scrobbleSucceeded();
    Q_EMIT statusChanged(QString("Submitted %1 scrobble(s)").arg(tracks.size()));
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::onNowPlayingError(int error, const QString& message)
{
    Q_EMIT statusChanged(QString("Now Playing error: %1").arg(message));
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::onScrobbleError(int error, const QString& message)
{
    Q_EMIT scrobbleFailed(message);
    Q_EMIT statusChanged(QString("Scrobble error: %1").arg(message));
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::processOfflineQueue()
{
    if(!m_scrobbler || m_offlineQueue.isEmpty())
    {
        return;
    }

    while(!m_offlineQueue.isEmpty())
    {
        ScrobbleInfo info = m_offlineQueue.dequeue();
        scrobble(info.trackInfo, info.timestamp);
    }

    saveOfflineQueue();
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::loadSettings()
{
    QSettings settings;

    settings.beginGroup("lastfm");
    m_enabled = settings.value("enabled", false).toBool();
    m_username = settings.value("username", "").toString();
    m_sessionKey = settings.value("sessionKey", "").toString();
    // Note: API key and secret are now embedded in the application
    /* OLD CODE - Load API keys from settings (kept for reference/debugging)
    m_apiKey = settings.value("apiKey", "").toString();
    m_apiSecret = settings.value("apiSecret", "").toString();
    */
    settings.endGroup();

    loadOfflineQueue();
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::saveSettings()
{
    QSettings settings;

    settings.beginGroup("lastfm");
    settings.setValue("enabled", m_enabled);
    settings.setValue("username", m_username);
    settings.setValue("sessionKey", m_sessionKey);
    // Note: API key and secret are now embedded in the application
    /* OLD CODE - Save API keys to settings (kept for reference/debugging)
    settings.setValue("apiKey", m_apiKey);
    settings.setValue("apiSecret", m_apiSecret);
    */
    settings.endGroup();
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::saveOfflineQueue()
{
    QSettings settings;

    settings.beginGroup("lastfm");
    settings.beginWriteArray("offlineQueue");

    for(int i = 0; i < m_offlineQueue.size(); ++i)
    {
        settings.setArrayIndex(i);
        const ScrobbleInfo& info = m_offlineQueue.at(i);

        if(info.trackInfo)
        {
            settings.setValue("artist", info.trackInfo->artist());
            settings.setValue("title", info.trackInfo->title());
            settings.setValue("album", info.trackInfo->album());
            settings.setValue("track", info.trackInfo->track());
            settings.setValue("timestamp", info.timestamp);
        }
    }

    settings.endArray();
    settings.endGroup();
}

//-------------------------------------------------------------------------------------------

void LastFMScrobbler::loadOfflineQueue()
{
    QSettings settings;

    m_offlineQueue.clear();

    settings.beginGroup("lastfm");
    int size = settings.beginReadArray("offlineQueue");

    for(int i = 0; i < size; ++i)
    {
        settings.setArrayIndex(i);

        // Note: We can't fully restore track info from settings, so we skip offline queue loading
        // A full implementation would need to serialize more track metadata
    }

    settings.endArray();
    settings.endGroup();
}

//-------------------------------------------------------------------------------------------
} // namespace player
} // namespace omega
//-------------------------------------------------------------------------------------------
