//-------------------------------------------------------------------------------------------
#ifndef __OMEGA_PLAYER_SETTINGSLASTFM_H
#define __OMEGA_PLAYER_SETTINGSLASTFM_H
//-------------------------------------------------------------------------------------------

#include "player/inc/SettingsBase.h"
#include "player/inc/LastFMScrobbler.h"
#include "player/ui_SettingsLastFM.h"

#include <QSharedPointer>

//-------------------------------------------------------------------------------------------
namespace omega
{
namespace player
{
//-------------------------------------------------------------------------------------------

class SettingsLastFM : public SettingsBase
{
    public:
        Q_OBJECT

    public:
        SettingsLastFM(LastFMScrobbler *scrobbler, QWidget *parent = 0, Qt::WindowFlags f = Qt::WindowFlags());
        virtual ~SettingsLastFM();

        virtual void onSelected(int index);

    protected:
        Ui::SettingsLastFM ui;
        LastFMScrobbler *m_scrobbler;

        void loadSettings();
        void saveSettings();
        void updateUI();

    protected Q_SLOTS:
        void onEnabledChanged(int state);
        void onAuthenticate();
        void onLogout();
        void onAuthenticationSucceeded();
        void onAuthenticationFailed(const QString& error);
        void onStatusChanged(const QString& status);
};

//-------------------------------------------------------------------------------------------
} // namespace player
} // namespace omega
//-------------------------------------------------------------------------------------------
#endif
//-------------------------------------------------------------------------------------------
