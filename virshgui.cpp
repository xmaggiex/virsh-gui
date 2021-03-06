#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <QGroupBox>
#include <QLabel>
#include <QTableWidget>
#include <QPushButton>
#include "virshgui.h"
#include "ui_virshgui.h"
#include "sshcommunication.h"
#include "basic-xml-syntax-highlighter/basic-xml-syntax-highlighter/BasicXMLSyntaxHighlighter.h"
#include "vncclient/vncclientwidgetcls.h"
#include "vncclient/rfbclientwidgetcls.h"
#include "vncclient/vncclientwidget2cls.h"

using namespace std;

string join(vector<string> list, const char* delim)
{
    if (list.empty()) {
        return "-";
    }

    stringstream liststream;
    copy(list.begin(), list.end(),
            ostream_iterator<string>(liststream, delim));
    string joinstr = liststream.str();
    joinstr.erase(joinstr.end() - 2);

    return joinstr;
}

/*
string replace(string s, char from, char to)
{
    for (string::iterator it = s.begin(); it != s.end(); ++it) {
        if (*it == from) {
            *it = to;
        }
    }

    return s;
}
*/

VirshGui::VirshGui(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::VirshGui)
{
    ui->setupUi(this);
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);

    //ui->vmListTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->vmListTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->vmListTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->vmListTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->vmListTable->setHorizontalHeaderLabels(QStringList() << "ID" << "Name" << "Status");
    ui->vmListTable->verticalHeader()->hide();
    ui->startStopButton->setEnabled(false);
    ui->shutdownButton->setEnabled(false);
    ui->rebootButton->setEnabled(false);
    ui->virtViewerButton->setEnabled(false);

    ui->xmlDisplay->createStandardContextMenu();

    populateBookmarkList();

    connect(ui->connectButton, SIGNAL(clicked(bool)), this, SLOT(makeSSHConnection()));
    connect(ui->bookmarList, SIGNAL(currentIndexChanged(int)), this, SLOT(fillLoginForm(int)));
    connect(ui->vmListTable, SIGNAL(cellClicked(int, int)), this, SLOT(vmChosen(int, int)));
    connect(ui->refreshVmList, SIGNAL(clicked()), this, SLOT(refreshVmList()));
    connect(ui->startStopButton, SIGNAL(clicked(bool)), this, SLOT(toggleVMStatus()));
    connect(ui->shutdownButton, SIGNAL(clicked(bool)), this, SLOT(shutdownVM()));
    connect(ui->rebootButton, SIGNAL(clicked(bool)), this, SLOT(rebootVM()));
    connect(ui->vmFilterEdit, SIGNAL(textChanged(QString)), this, SLOT(filterVMs(QString)));
    connect(ui->virtViewerButton, SIGNAL(clicked(bool)), this, SLOT(vncDisplay()));
    connect(ui->saveXMLButton, SIGNAL(clicked(bool)), this, SLOT(savexml()));
    //connect(ui->testButton, SIGNAL(clicked(bool)), this, SLOT(testButton()));
}

VirshGui::~VirshGui()
{
    delete ui;
}

void VirshGui::makeSSHConnection()
{
    string user = ui->userEdit->text().toStdString();
    string password = ui->passwordEdit->text().toStdString();
    string host = ui->hostEdit->text().toStdString();
    int port = ui->portEdit->text().toInt();
    ssh = new SSHCommunication(user, password, host, port);
    ui->refreshVmList->setEnabled(true);

    vmlist = ssh->listVMs();
    populateVMList(vmlist);
}

void VirshGui::populateVMList(map<string, VM> vmlist)
{
    bool listIsEmpty = ui->vmListTable->rowCount() == 0;
    int row = 0;
    for (auto vm : vmlist) {
        if (listIsEmpty) {
            ui->vmListTable->setRowCount(row + 1);
        }
        string strstatus = VM::statusToString(vm.second.getStatus());
        //cout << vm.second.getID() << " " << vm.second.getName() << " " << strstatus << endl;

        QTableWidgetItem *id = new QTableWidgetItem(
                QString::fromStdString(vm.second.getID()));
        QTableWidgetItem *name = new QTableWidgetItem(
                QString::fromStdString(vm.second.getName()));
        QTableWidgetItem *status = new QTableWidgetItem(
                QString::fromStdString(strstatus));

        ui->vmListTable->setItem(row, 0, id);
        ui->vmListTable->setItem(row, 1, name);
        ui->vmListTable->setItem(row, 2, status);

        row++;
    }
}

void VirshGui::populateBookmarkList()
{
    ifstream bookmarkFile;
    string line;

    bookmarkFile.open("bookmarks.xml", ios::in);
    if (! bookmarkFile.is_open()) {
        return;
    }

    ui->bookmarList->addItem("");
    while (getline(bookmarkFile, line)) {
        istringstream linestream(line);
        ostringstream out;
        string host, port, user, password;

        linestream >> host >> port >> user >> password;

        if (! host.empty() && host[0] != '#') {
            //std::cout << host << std::endl;
            out << user << "@" << host << ":" << port;
            ui->bookmarList->addItem(out.str().c_str());
        }
    }

    bookmarkFile.close();
}

void VirshGui::filterVMs(QString filter)
{
    for (int row = 0; row < ui->vmListTable->rowCount(); ++row) {
        QTableWidgetItem *vmnameItem = ui->vmListTable->item(row, 1);
        bool match = vmnameItem->text()
            .toLower()
            .contains(filter.toLower());
        ui->vmListTable->setRowHidden(row, !match);
    }
}

void VirshGui::vncDisplay()
{
    string vmname = ui->vmnameLabel->text().toStdString();
    VM vm = vmlist[vmname];
    int vncport = stoi(vm.getVNCPort());
    vncclientwidget2cls *vnc = new vncclientwidget2cls();
    vnc->connectVNCTCP(QString::fromStdString(ssh->getHost()), vncport);
    vnc->showNormal();
}

void VirshGui::toggleVMStatus()
{
    string vmname = ui->vmnameLabel->text().toStdString();
    VM vm = vmlist[vmname];
    if (vm.getStatus() == VMStatus::shutoff) {
        vm.start();
    } else if (vm.getStatus() == VMStatus::running) {
        vm.destroy();
    }

    refreshVmList();
}

void VirshGui::shutdownVM()
{
    string vmname = ui->vmnameLabel->text().toStdString();
    VM vm = vmlist[vmname];
    if (vm.getStatus() == VMStatus::running) {
        vm.shutdown();
    }
}
void VirshGui::rebootVM()
{
    string vmname = ui->vmnameLabel->text().toStdString();
    VM vm = vmlist[vmname];
    if (vm.getStatus() == VMStatus::running) {
        vm.reboot();
    }
}

void VirshGui::refreshVmList()
{
    //std::cout << "refreshVmList" << std::endl;
    vmlist = ssh->listVMs();
    populateVMList(vmlist);
    filterVMs(ui->vmFilterEdit->text());
    if (! ui->vmnameLabel->text().isEmpty()) {
        populateVMInfos(ui->vmnameLabel->text().toStdString());
    }

    //ui->statusBar->showMessage("VM Liste aktualisiert", 2000);
}

void VirshGui::fillLoginForm(int hostidx)
{
    ifstream bookmarkFile;
    string line;
    string host, port, user, password;
    int count = 1;

    if (hostidx == 0) {
        ui->hostEdit->setText("");
        ui->portEdit->setText("");
        ui->userEdit->setText("");
        ui->passwordEdit->setText("");

        return;
    }

    bookmarkFile.open("bookmarks.xml", ios::in);
    if (! bookmarkFile.is_open()) {
        return;
    }


    while (getline(bookmarkFile, line) && count <= hostidx) {
        //std::cout << count << ": " << line << std::endl;
        istringstream linestream(line);
        linestream >> host >> port >> user >> password;
        if (host[0] != '#' && ! line.empty()) {
            count++;
        }
    }

    ui->hostEdit->setText(host.c_str());
    ui->portEdit->setText(port.c_str());
    ui->userEdit->setText(user.c_str());
    ui->passwordEdit->setText(password.c_str());

    bookmarkFile.close();
}

void VirshGui::populateVMInfos(string vmname)
{
    VMStatus vmstatus = vmlist[vmname].getStatus();
    string strstatus = vmlist[vmname].statusToString(vmstatus);
    string memory = vmlist[vmname].getMemory();
    string vmxml = vmlist[vmname].dumpXML();
    string cpuCount = vmlist[vmname].getCPUCount();
    string ostype = vmlist[vmname].getOSType();
    string arch = vmlist[vmname].getArch();
    vector<string> bootDevs = vmlist[vmname].getBootDevs();
    vector<string> hvFeatures = vmlist[vmname].getHVFeatures();
    vector<string> cpuFeatures = vmlist[vmname].getCPUFeatures();

    string bootDevStr = join(bootDevs, ", ");
    string hvFeatureStr = join(hvFeatures, ", ");
    string cpuFeatureStr = join(cpuFeatures, ", ");

    if (vmstatus == VMStatus::shutoff) {
        ui->startStopButton->setText("VM starten");
        ui->startStopButton->setEnabled(true);
        ui->shutdownButton->setDisabled(true);
        ui->rebootButton->setDisabled(true);
        ui->virtViewerButton->setDisabled(true);
    } else if (vmstatus == VMStatus::running) {
        ui->startStopButton->setText("VM ausschalten");
        ui->startStopButton->setEnabled(true);
        ui->shutdownButton->setEnabled(true);
        ui->rebootButton->setEnabled(true);
        ui->virtViewerButton->setEnabled(true);
    } else {
        ui->startStopButton->setText("keine Aktion");
        ui->startStopButton->setDisabled(true);
        ui->shutdownButton->setDisabled(true);
        ui->rebootButton->setDisabled(true);
        ui->virtViewerButton->setDisabled(true);
    }

    while (ui->snapshotsTabLayout->count() > 0) {
        QLayoutItem *item = ui->snapshotsTabLayout->takeAt(0);
        delete item->widget();
        delete item;
    }

    int snapshotTableCount = 0;
    for (auto hdd : vmlist[vmname].getHDDImages()) {
        int row = 0;
        QGroupBox *hddGroupBox = new QGroupBox(QString::fromStdString(hdd.getPath()));
        hddGroupBox->setFlat(false);

        QVBoxLayout *hddVBox = new QVBoxLayout;
        QTableWidget *snapshotTable = new QTableWidget(0, 5, this);
        snapshotTable->setSelectionMode(QAbstractItemView::SingleSelection);
        snapshotTable->setHorizontalHeaderLabels(QStringList() << "ID" << "Tag" << "VM Size" << "Date" << "VM Clock");
        snapshotTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        //snapshotTable->setColumnWidth(0, 45);
        snapshotTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        snapshotTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        snapshotTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        snapshotTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        snapshotTable->verticalHeader()->hide();
        snapshotTable->setAlternatingRowColors(true);
        //snapshotTable->setSortingEnabled(true);
        snapshotTable->setEditTriggers(QTableWidget::NoEditTriggers);
        connect(snapshotTable, &QTableWidget::cellPressed, [this, snapshotTableCount](){ clearSnapshotSelectionsExcept(snapshotTableCount); });
        snapshotTableCount++;

        for (auto snapshot : hdd.getSnapshots()) {
            snapshotTable->setRowCount(row + 1);
            QTableWidgetItem *id = new QTableWidgetItem(
                    QString::fromStdString(snapshot.getID()));
            QTableWidgetItem *tag = new QTableWidgetItem(
                    QString::fromStdString(snapshot.getTag()));
            QTableWidgetItem *size = new QTableWidgetItem(
                    QString::fromStdString(snapshot.getSize()));
            QTableWidgetItem *date = new QTableWidgetItem(
                    QString::fromStdString(snapshot.getDate()));
            QTableWidgetItem *clock = new QTableWidgetItem(
                    QString::fromStdString(snapshot.getClock()));

            snapshotTable->setItem(row, 0, id);
            snapshotTable->setItem(row, 1, tag);
            snapshotTable->setItem(row, 2, size);
            snapshotTable->setItem(row, 3, date);
            snapshotTable->setItem(row, 4, clock);

            row++;
        }
        hddVBox->addWidget(snapshotTable);

        QHBoxLayout *addSnapHBox = new QHBoxLayout;
        QLineEdit *addSnapNameEdit = new QLineEdit;
        QPushButton *addSnapButton = new QPushButton;
        addSnapButton->setText("Snapshot erstellen");
        addSnapButton->setEnabled(false);
        string hddPath = hdd.getPath();
        connect(addSnapNameEdit, &QLineEdit::textChanged, [this, addSnapButton, addSnapNameEdit](){ toggleAddSnapshotButton(addSnapButton, addSnapNameEdit); });
        connect(addSnapButton, &QPushButton::clicked, [this, hddPath, vmname, addSnapNameEdit](){ createSnapshot(hddPath, vmname, addSnapNameEdit->text().toStdString()); });
        addSnapHBox->addWidget(addSnapNameEdit);
        addSnapHBox->addWidget(addSnapButton);

        hddVBox->addLayout(addSnapHBox);
        hddGroupBox->setLayout(hddVBox);
        ui->snapshotsTabLayout->addWidget(hddGroupBox);
    }

    QPushButton *applySnapshotButton = new QPushButton("Snapshot Anwenden", this);
    connect(applySnapshotButton, SIGNAL(clicked(bool)), this, SLOT(applySnapshot()));
    //connect(applySnapshotButton, &QPushButton::clicked, [this](){ clearSnapshotSelectionsExcept(1); });
    ui->snapshotsTabLayout->addWidget(applySnapshotButton);

    ui->xmlDisplay->setText(QString::fromStdString(vmxml));
    ui->vmnameLabel->setText(QString::fromStdString(vmname));
    ui->statusLabel->setText(QString::fromStdString(strstatus));
    ui->memoryLabel->setText(QString::fromStdString(memory));
    ui->cpucountLabel->setText(QString::fromStdString(cpuCount));
    ui->typeLabel->setText(QString::fromStdString(ostype));
    ui->archLabel->setText(QString::fromStdString(arch));
    ui->bootdevLabel->setText(QString::fromStdString(bootDevStr));
    ui->hvFeaturesLabel->setText(QString::fromStdString(hvFeatureStr));
    ui->hvFeaturesLabel->setWordWrap(true);
    ui->cpuFeaturesLabel->setText(QString::fromStdString(cpuFeatureStr));
    ui->cpuFeaturesLabel->setWordWrap(true);
}

void VirshGui::clearSnapshotSelectionsExcept(int snapshotTableIndex)
{
    int count = 0;
    for (auto snapshotTable : ui->snapshotsTab->findChildren<QTableWidget *>()) {
        if (count == snapshotTableIndex) {
            count++;
            continue;
        }
        snapshotTable->clearSelection();
        count++;
    }
}

void VirshGui::applySnapshot()
{
    int count = 0;
    for (auto hddGroupBox : ui->snapshotsTab->findChildren<QGroupBox *>()) {
        QTableWidget *snapshotTable = hddGroupBox->findChild<QTableWidget *>();
        QList<QTableWidgetItem *> items = snapshotTable->selectedItems();
        //std::cout << items.count() << std::endl;
        if (items.count() == 1) {
            //cout
            //    << "hdd: "
            //    << hddGroupBox->title().toStdString()
            //    << ", snapshot: "
            //    << snapshotTable->item(items.at(0)->row(), 1)->text().toStdString()
            //    << endl;
            string snapshotID = snapshotTable->item(items.at(0)->row(), 0)->text().toStdString();
            string hddPath = hddGroupBox->title().toStdString();
            string cmd = "qemu-img snapshot -a '" + snapshotID + "' '" + hddPath + "'";
            string execOut = ssh->execCmd(cmd);
            if (ssh->getLastExitCode() == 0) {
                ui->statusBar->showMessage("Snapshot erfolgreich angewandt", 5000);
            } else {
                ui->statusBar->showMessage(
                        QString::fromStdString("Fehler: " + execOut),
                        5000);
            }
            break;
        }
        count++;
    }
}

void VirshGui::createSnapshot(string hddPath, string vmname, string snapshotName)
{
    if (snapshotName.empty()) {
        ui->statusBar->showMessage("Bitte Snapshotnamen angeben", 5000);
        return;
    }
    string cmd = "qemu-img snapshot -c '" + snapshotName + "' '" + hddPath + "'";
    string execOut = ssh->execCmd(cmd);
    if (ssh->getLastExitCode() == 0) {
        ui->statusBar->showMessage("Snapshot Erstellt", 5000);
    } else {
        ui->statusBar->showMessage(
                        QString::fromStdString("Fehler: " + execOut),
                        5000);
    }
    populateVMInfos(vmname);
}

void VirshGui::vmChosen(int row, int column)
{
    Q_UNUSED(column);

    BasicXMLSyntaxHighlighter *highlighter = new BasicXMLSyntaxHighlighter(ui->xmlDisplay);
    Q_UNUSED(highlighter);

    string vmname = ui->vmListTable->item(row, 1)->text().toStdString();
    populateVMInfos(vmname);
}

void VirshGui::toggleAddSnapshotButton(QPushButton *addSnapButton, QLineEdit *addSnapNameEdit)
{
    if (addSnapNameEdit->text().isEmpty()) {
        addSnapButton->setEnabled(false);
    } else {
        addSnapButton->setEnabled(true);
    }
}

void VirshGui::savexml()
{
    string vmname = ui->vmnameLabel->text().toStdString();
    VM vm = vmlist[vmname];
    vm.changeXML(ui->xmlDisplay->toPlainText().toStdString());

    string out = ssh->getLastStdout();

    std::cout << out << std::endl;
    std::cout << out.find("(null)") << std::endl;
    if (out.find("(null)") == string::npos) {
        ui->statusBar->showMessage(QString::fromStdString(out), 5000);
    } else {
        ui->statusBar->showMessage("Error couldn't save or wrong XML", 5000);
    }
}

void VirshGui::testButton()
{
    std::cout << ssh->execCmd("/home/steven/test.sh") << std::endl;
    std::cout << ssh->getLastExitCode() << std::endl;
}
