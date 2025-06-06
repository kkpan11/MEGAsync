#ifndef TRANSFERNOTIFICATIONBUILDER_H
#define TRANSFERNOTIFICATIONBUILDER_H

#include "DesktopNotifications.h"
#include "TransferMetaData.h"

#include <QObject>

class TransferNotificationBuilder : public QObject
{
    Q_OBJECT

public:
    TransferNotificationBuilder(const std::shared_ptr<TransferMetaData>& data);
    ~TransferNotificationBuilder() = default;

    DesktopNotifications::NotificationInfo buildNotification(const bool isLogged);

    static QString getDownloadFailedTitle();
    static QString getDownloadSomeFailedTitle();
    static QString getDownloadSuccessTitle();

    static QString getDownloadFailedText(int num, const QString &destPath);
    static QString getSomeDownloadFailedText(int completed, int failed);
    static QString getDownloadSuccessText(int num, const QString& destPath);
    static QString getSingleDownloadFailed(const QString& fileName, const QString& destPath);

    static QString getShowInFolderText();


protected:
    QString buildUploadTitle();
    QString buildDownloadTitle();
    QString buildImportedLinkErrorTitle();

    QString buildSingleUploadMessage();
    QStringList buildSingleUploadActions();

    QString buildSingleDownloadMessage(const QString& destinationPath);
    QStringList buildSingleDownloadActions(const QString &);

    QString buildMultipleUploadMessage();
    QStringList buildMultipleUploadActions();

    QString buildMultipleDownloadMessage(const QString& destinationPath);
    QStringList buildMultipleDownloadActions(const QString &);

    QString buildSingleNonExistentDataMessageUpload(const QString& itemName);
    QString buildSingleNonExistentDataMessageDownload(const QString& itemName);
    QString buildNonExistentItemsMessageUploads();
    QString buildNonExistentItemsMessageDownloads();

    QString buildImportedLinkError(std::shared_ptr<TransferMetaDataItem> failedItem);

    QString getImagePath();

    bool isFolder() const;

    const std::shared_ptr<TransferMetaData> mData;
};

#endif // TRANSFERNOTIFICATIONBUILDER_H
