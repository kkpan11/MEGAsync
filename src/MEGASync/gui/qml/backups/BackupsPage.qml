import QtQuick 2.15

import components.steps 1.0

import BackupCandidates 1.0

BackupsFlow {
    id: root

    required property StepPanel stepPanelRef
    required property var backupsContentItemRef

    selectFoldersPage: Component {
        id: selectFoldersPageComponent

        SelectFoldersPage {
            id: selectFoldersPageItem
        }
    }

    confirmFoldersPage: Component {
        id: confirmFoldersPageComponent

        ConfirmFoldersPage {
            id: confirmFoldersPageItem
        }
    }

    Item {
        id: stepPanelStateWrapper

        readonly property string selectBackupPage: "selectBackupPage"
        readonly property string confirmBackupPage: "confirmBackupPage"

        states: [
            State {
                name: stepPanelStateWrapper.selectBackupPage
                PropertyChanges { target: root.stepPanelRef; state: root.stepPanelRef.step1; }
            },
            State {
                name: stepPanelStateWrapper.confirmBackupPage
                PropertyChanges {
                    target: root.stepPanelRef;
                    state: {
                        if(backupCandidatesAccess.globalError !== BackupCandidates.NONE) {
                            if(backupCandidatesAccess.globalError === BackupCandidates.SDK_CREATION) {
                                return root.stepPanelRef.step2Error;
                            }
                            else {
                                return root.stepPanelRef.step2Warning;
                            }
                        }
                        else {
                            return root.stepPanelRef.step2;
                        }
                    }
                }
            }
        ]
    }

    onBackupFlowMoveToFinal: (success) => {
        if (success) {
            backupsContentItemRef.state = backupsContentItemRef.resume;
        }
    }

    onStateChanged: {
        switch(root.state) {
            case root.selectBackup:
                stepPanelStateWrapper.state = stepPanelStateWrapper.selectBackupPage;
                break;
            case root.confirmBackup:
                stepPanelStateWrapper.state = stepPanelStateWrapper.confirmBackupPage;
                break;
            default:
                console.warn("BackupsPage window: state does not exist -> " + root.state);
                break;
        }
    }

}
