#include "TransferQuota.h"

#include "DialogOpener.h"
#include "mega/types.h"
#include "OverQuotaDialog.h"
#include "Platform.h"
#include "StatsEventHandler.h"
#include "UpsellComponent.h"

TransferQuota::TransferQuota(std::shared_ptr<DesktopNotifications> desktopNotifications):
    mMegaApi(MegaSyncApp->getMegaApi()),
    mPreferences(Preferences::instance()),
    mOsNotifications{std::move(desktopNotifications)},
    mQuotaState{QuotaState::OK},
    mWaitTimeUntil{std::chrono::system_clock::time_point()},
    overQuotaAlertVisible{false},
    almostQuotaAlertVisible{false}
{
    updateQuotaState();
}

void TransferQuota::setOverQuota(std::chrono::milliseconds waitTime)
{
    mWaitTimeUntil = std::chrono::system_clock::now() + waitTime;
    mQuotaState = QuotaState::OVERQUOTA;
    emit sendState(QuotaState::OVERQUOTA);
    checkExecuteAlerts();
}

bool TransferQuota::isOverQuota()
{
    updateQuotaState();
    return (mQuotaState == QuotaState::OVERQUOTA);
}

bool TransferQuota::isQuotaWarning()
{
    updateQuotaState();
    return (mQuotaState == QuotaState::WARNING);
}

bool TransferQuota::isQuotaFull()
{
    updateQuotaState();
    return (mQuotaState == QuotaState::FULL);
}

void TransferQuota::updateQuotaState()
{
    if (mPreferences->logged())
    {
        auto newState (mQuotaState);

        // Check if the overquota timeout given by the API has expired
        if (mQuotaState == QuotaState::OVERQUOTA && std::chrono::system_clock::now() >= mWaitTimeUntil)
        {
            newState = QuotaState::OK;
            mWaitTimeUntil = std::chrono::system_clock::time_point();
            mPreferences->clearTemporalBandwidth();
            emit waitTimeIsOver();
        }

        // We have 2 cases:
        // Unlimited (0) totalBytes (ex. Free account) --> makes no sense to check %, OQ given
        //                                                 for a limited time by API
        // Limited (>0) totalBytes --> check % and set according status. OVERQUOTA trumps FULL
        const auto totalBytes (mPreferences->totalBandwidth());
        if (totalBytes > 0)
        {
            const auto usedBytes (mPreferences->usedBandwidth());
            const auto usagePercent (Utilities::partPer(usedBytes, totalBytes, 100));

            if (usagePercent >= FULL_QUOTA_PER_CENT)
            {
                // Overquota possible only if >= FULL
                if (newState != QuotaState::OVERQUOTA)
                {
                    newState = QuotaState::FULL;
                }
            }
            else if (usagePercent >= ALMOST_OVER_QUOTA_PER_CENT)
            {
                newState = QuotaState::WARNING;
            }
            else
            {
                newState = QuotaState::OK;
            }
        }

        // Emit new state and take action accordingly
        if (newState != mQuotaState)
        {
            mQuotaState = newState;
            emit sendState(mQuotaState);
            if (newState == QuotaState::OK)
            {
                closeUpsellTransferDialog();
            }
            else
            {
                checkExecuteAlerts();
            }
        }
    }
}

void TransferQuota::checkExecuteDialog()
{
    const auto disabledUntil(mPreferences->getTransferOverQuotaDialogLastExecution() +
                             Preferences::OVER_QUOTA_DIALOG_DISABLE_DURATION);
    const bool dialogExecutionEnabled(std::chrono::system_clock::now() >= disabledUntil);
    if (dialogExecutionEnabled)
    {
        mPreferences->setTransferOverQuotaDialogLastExecution(std::chrono::system_clock::now());
        MegaSyncApp->getStatsEventHandler()->sendEvent(
            AppStatsEvents::EventType::TRSF_OVER_QUOTA_DIAL);

        const auto endWaitTimeSinceEpoch(mWaitTimeUntil.time_since_epoch());
        const auto endWaitTimeSinceEpochSeconds(
            std::chrono::duration_cast<std::chrono::seconds>(endWaitTimeSinceEpoch).count());

        MegaSyncApp->showUpsellDialog(UpsellPlans::ViewMode::TRANSFER_EXCEEDED);
        if (auto dialogInfo = DialogOpener::findDialog<QmlDialogWrapper<UpsellComponent>>())
        {
            dialogInfo->getDialog()->wrapper()->setTransferFinishTime(endWaitTimeSinceEpochSeconds);
        }
    }
}

void TransferQuota::checkExecuteNotification()
{
    const auto disabledUntil = mPreferences->getTransferOverQuotaOsNotificationLastExecution()+Preferences::OVER_QUOTA_OS_NOTIFICATION_DISABLE_DURATION;
    const bool notificationExecutionEnabled{std::chrono::system_clock::now() >= disabledUntil};
    if (notificationExecutionEnabled)
    {
        mPreferences->setTransferOverQuotaOsNotificationLastExecution(std::chrono::system_clock::now());
        MegaSyncApp->getStatsEventHandler()->sendEvent(AppStatsEvents::EventType::TRSF_OVER_QUOTA_NOTIF);
        sendOverQuotaOsNotification();
    }
}

void TransferQuota::checkExecuteUiMessage()
{
    const auto disabledUntil = mPreferences->getTransferOverQuotaUiAlertLastExecution()+Preferences::OVER_QUOTA_UI_ALERT_DISABLE_DURATION;
    const bool uiAlertExecutionEnabled{std::chrono::system_clock::now() >= disabledUntil};
    if (uiAlertExecutionEnabled)
    {
        if (!overQuotaAlertVisible) //We only want to send the event when transitioning from non-visible alert message
        {
            MegaSyncApp->getStatsEventHandler()->sendEvent(AppStatsEvents::EventType::TRSF_OVER_QUOTA_MSG);
        }

        emit overQuotaMessageNeedsToBeShown();
    }
}

void TransferQuota::checkExecuteWarningOsNotification()
{
    const auto disabledUntil = mPreferences->getTransferAlmostOverQuotaOsNotificationLastExecution()+Preferences::ALMOST_OVER_QUOTA_OS_NOTIFICATION_DISABLE_DURATION;
    const bool notificationExecutionEnabled{std::chrono::system_clock::now() >= disabledUntil};
    if (notificationExecutionEnabled)
    {
        mPreferences->setTransferAlmostOverQuotaOsNotificationLastExecution(std::chrono::system_clock::now());
        MegaSyncApp->getStatsEventHandler()->sendEvent(AppStatsEvents::EventType::TRSF_ALMOST_OVERQUOTA_NOTIF);
        sendQuotaWarningOsNotification();
    }
}

void TransferQuota::checkExecuteWarningUiMessage()
{
    const auto disabledUntil = mPreferences->getTransferAlmostOverQuotaUiAlertLastExecution()+Preferences::OVER_QUOTA_UI_ALERT_DISABLE_DURATION;
    const bool executeUiWarningAlert{std::chrono::system_clock::now() >= disabledUntil};
    if (executeUiWarningAlert)
    {
        if (!almostQuotaAlertVisible)
        {
            MegaSyncApp->getStatsEventHandler()->sendEvent(AppStatsEvents::EventType::TRSF_ALMOST_OVER_QUOTA_MSG);
        }

        emit almostOverQuotaMessageNeedsToBeShown();
    }
}

void TransferQuota::checkExecuteAlerts()
{
    const MegaApplication* megaApp{static_cast<MegaApplication*>(qApp)};
    const bool allowAlerts{megaApp->isInfoDialogVisible() ||
                           Platform::getInstance()->isUserActive()};
    const bool userLogged{mPreferences && mPreferences->logged()};
    const bool bandwidthAlertsEnabled{!megaApp->finished() && userLogged && allowAlerts};
    if (bandwidthAlertsEnabled)
    {
        if (isOverQuota())
        {
            checkExecuteDialog();
            checkExecuteNotification();
            checkExecuteUiMessage();
        }
        else if (isQuotaWarning())
        {
            checkExecuteWarningOsNotification();
            checkExecuteWarningUiMessage();
        }
    }
}

void TransferQuota::checkQuotaAndAlerts()
{
    isOverQuota();
    checkExecuteAlerts();
}

void TransferQuota::checkImportLinksAlertDismissed(std::function<void(int)> func)
{
    checkAlertDismissed(OverQuotaDialogType::BANDWIDTH_IMPORT_LINK, func);
}

void TransferQuota::checkDownloadAlertDismissed(std::function<void(int)> func)
{
    checkAlertDismissed(OverQuotaDialogType::BANDWIDTH_DOWNLOAD, func);
}

void TransferQuota::checkStreamingAlertDismissed(std::function<void(int)> func)
{
    checkAlertDismissed(OverQuotaDialogType::BANDWIDTH_STREAM, func);
}

void TransferQuota::checkAlertDismissed(OverQuotaDialogType type, std::function<void(int)> func)
{
    if(isOverQuota())
    {
        auto dialog = OverQuotaDialog::showDialog(type);
        if(dialog)
        {
            DialogOpener::showDialog(dialog, [dialog, func]()
            {
               func(dialog->result());
            });

            return;
        }
    }

    func(QDialog::Rejected);
}

void TransferQuota::closeUpsellTransferDialog()
{
    if (auto dialog = DialogOpener::findDialog<QmlDialogWrapper<UpsellComponent>>())
    {
        auto viewMode(dialog->getDialog()->wrapper()->viewMode());
        if (viewMode == UpsellPlans::ViewMode::TRANSFER_EXCEEDED)
        {
            dialog->close();
        }
    }
}

QTime TransferQuota::getRemainingTransferQuotaTime()
{
    auto remainingDuration = std::chrono::duration_cast<std::chrono::seconds>(
        mWaitTimeUntil - std::chrono::system_clock::now());
    int remainingSeconds = static_cast<int>(remainingDuration.count());

    if (remainingSeconds < 0)
    {
        remainingSeconds = 0;
    }

    return QTime(0, 0, 0).addSecs(remainingSeconds);
}

void TransferQuota::reset()
{
    overQuotaAlertVisible = false;
    almostQuotaAlertVisible = false;
    mQuotaState = QuotaState::OK;
    mWaitTimeUntil = std::chrono::system_clock::time_point();
}

QuotaState TransferQuota::quotaState() const
{
    return mQuotaState;
}

void TransferQuota::sendQuotaWarningOsNotification()
{
    const QString title{tr("Limited available transfer quota.")};
    mOsNotifications->sendOverTransferNotification(title);
}

void TransferQuota::sendOverQuotaOsNotification()
{
    const QString title{tr("Depleted transfer quota.")};
    mOsNotifications->sendOverTransferNotification(title);
}

void TransferQuota::onTransferOverquotaVisibilityChange(bool messageShown)
{
    if (!messageShown)
    {
        mPreferences->setTransferOverQuotaUiAlertLastExecution(std::chrono::system_clock::now());
    }

    overQuotaAlertVisible = messageShown;
}

void TransferQuota::onAlmostTransferOverquotaVisibilityChange(bool messageShown)
{
    if (!messageShown)
    {
        mPreferences->setTransferAlmostOverQuotaUiAlertLastExecution(std::chrono::system_clock::now());
    }

    almostQuotaAlertVisible = messageShown;
}
