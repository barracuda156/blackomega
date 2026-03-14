#include "player/inc/SettingsLastFM.h"
#include "player/inc/Settings.h"

#include <QMessageBox>

//-------------------------------------------------------------------------------------------
namespace omega
{
namespace player
{
//-------------------------------------------------------------------------------------------

SettingsLastFM::SettingsLastFM(LastFMScrobbler *scrobbler, QWidget *parent, Qt::WindowFlags f) : SettingsBase(parent, f),
    m_scrobbler(scrobbler)
{
    ui.setupUi(this);

    QObject::connect(ui.m_enabledCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onEnabledChanged(int)));
    QObject::connect(ui.m_authenticateButton, SIGNAL(clicked()), this, SLOT(onAuthenticate()));
    QObject::connect(ui.m_logoutButton, SIGNAL(clicked()), this, SLOT(onLogout()));

    if(m_scrobbler)
    {
        QObject::connect(m_scrobbler, SIGNAL(authenticationSucceeded()), this, SLOT(onAuthenticationSucceeded()));
        QObject::connect(m_scrobbler, SIGNAL(authenticationFailed(QString)), this, SLOT(onAuthenticationFailed(QString)));
        QObject::connect(m_scrobbler, SIGNAL(statusChanged(QString)), this, SLOT(onStatusChanged(QString)));
    }

    loadSettings();
    updateUI();
}

//-------------------------------------------------------------------------------------------

SettingsLastFM::~SettingsLastFM()
{
    saveSettings();
}

//-------------------------------------------------------------------------------------------

void SettingsLastFM::onSelected(int index)
{
    // Called when this tab is selected
    updateUI();
}

//-------------------------------------------------------------------------------------------

void SettingsLastFM::loadSettings()
{
    if(!m_scrobbler)
    {
        return;
    }

    ui.m_enabledCheckBox->setChecked(m_scrobbler->isEnabled());
    ui.m_usernameEdit->setText(m_scrobbler->username());
    // Note: API key and secret are now embedded in the application
}

//-------------------------------------------------------------------------------------------

void SettingsLastFM::saveSettings()
{
    if(!m_scrobbler)
    {
        return;
    }

    m_scrobbler->setEnabled(ui.m_enabledCheckBox->isChecked());
    // Note: Username is set by authentication response, not from UI
    // Note: API key and secret are now embedded in the application
}

//-------------------------------------------------------------------------------------------

void SettingsLastFM::updateUI()
{
    if(!m_scrobbler)
    {
        return;
    }

    bool enabled = ui.m_enabledCheckBox->isChecked();
    bool authenticated = m_scrobbler->isAuthenticated();

    // API credentials are now embedded in the application
    ui.m_authenticationGroup->setEnabled(enabled);
    ui.m_scrobblingGroup->setEnabled(enabled);

    ui.m_authenticateButton->setEnabled(enabled && !authenticated);
    ui.m_logoutButton->setEnabled(enabled && authenticated);

    ui.m_usernameEdit->setEnabled(!authenticated);
    ui.m_passwordEdit->setEnabled(!authenticated);

    if(authenticated)
    {
        ui.m_statusLabel->setText("Authenticated as " + m_scrobbler->username());
    }
    else
    {
        ui.m_statusLabel->setText("Not authenticated");
    }
}

//-------------------------------------------------------------------------------------------

void SettingsLastFM::onEnabledChanged(int state)
{
    saveSettings();
    updateUI();
}

//-------------------------------------------------------------------------------------------

void SettingsLastFM::onAuthenticate()
{
    if(!m_scrobbler)
    {
        return;
    }

    QString username = ui.m_usernameEdit->text().trimmed();
    QString password = ui.m_passwordEdit->text();

    if(username.isEmpty())
    {
        QMessageBox::warning(this, "Last.fm Authentication", "Please enter your Last.fm username.");
        return;
    }

    if(password.isEmpty())
    {
        QMessageBox::warning(this, "Last.fm Authentication", "Please enter your Last.fm password.");
        return;
    }

    ui.m_statusLabel->setText("Authenticating...");
    ui.m_authenticateButton->setEnabled(false);

    m_scrobbler->authenticate(username, password);
}

//-------------------------------------------------------------------------------------------

void SettingsLastFM::onLogout()
{
    if(!m_scrobbler)
    {
        return;
    }

    m_scrobbler->logout();
    ui.m_passwordEdit->clear();
    updateUI();
}

//-------------------------------------------------------------------------------------------

void SettingsLastFM::onAuthenticationSucceeded()
{
    saveSettings();
    updateUI();

    QMessageBox::information(this, "Last.fm Authentication",
                             "Successfully authenticated with Last.fm!");
}

//-------------------------------------------------------------------------------------------

void SettingsLastFM::onAuthenticationFailed(const QString& error)
{
    updateUI();

    QMessageBox::critical(this, "Last.fm Authentication Failed",
                          "Authentication failed: " + error);
}

//-------------------------------------------------------------------------------------------

void SettingsLastFM::onStatusChanged(const QString& status)
{
    ui.m_statusLabel->setText(status);
}

//-------------------------------------------------------------------------------------------
} // namespace player
} // namespace omega
//-------------------------------------------------------------------------------------------
