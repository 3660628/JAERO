#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

#ifdef __linux__
#include <unistd.h>
#define Sleep(x) usleep(x * 1000);
#endif
#ifdef _WIN32
#include <windows.h>
#endif


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //create the modulators, varicode encoder pipe, and serial ppt
    varicodepipeencoder = new VariCodePipeEncoder(this);
    audiomskmodulator = new AudioMskModulator(this);  
    aerol = new AeroL(this); //Create Aero L test sink

    //create settings dialog. only some modulator settings are held atm.
    settingsdialog = new SettingsDialog(this);

    //create the demodulator
    audiomskdemodulator = new AudioMskDemodulator(this);

    //create a udp socket and a varicode decoder pipe
    udpsocket = new QUdpSocket(this);
    varicodepipedecoder = new VariCodePipeDecoder(this);

    //default sink is the aerol device
    audiomskdemodulator->ConnectSinkDevice(aerol);

    //console setup
    ui->console->setEnabled(true);
    ui->console->setLocalEchoEnabled(true);

    //aerol setup
    aerol->ConnectSinkDevice(ui->console->consoledevice);

    //statusbar setup
    freqlabel = new QLabel();
    ebnolabel = new QLabel();
    ui->statusBar->addPermanentWidget(new QLabel());
    ui->statusBar->addPermanentWidget(freqlabel);
    ui->statusBar->addPermanentWidget(ebnolabel);

    //led setup
    ui->ledvolume->setLED(QIcon::Off);
    ui->ledsignal->setLED(QIcon::Off);

    //connections
    connect(audiomskdemodulator, SIGNAL(Plottables(double,double,double)),              this,SLOT(PlottablesSlot(double,double,double)));
    connect(audiomskdemodulator, SIGNAL(SignalStatus(bool)),                            this,SLOT(SignalStatusSlot(bool)));
    connect(audiomskdemodulator, SIGNAL(WarningTextSignal(QString)),                    this,SLOT(WarningTextSlot(QString)));
    connect(audiomskdemodulator, SIGNAL(EbNoMeasurmentSignal(double)),                  this,SLOT(EbNoSlot(double)));
    connect(audiomskdemodulator, SIGNAL(PeakVolume(double)),                            this, SLOT(PeakVolumeSlot(double)));
    connect(audiomskdemodulator, SIGNAL(OrgOverlapedBuffer(QVector<double>)),           ui->spectrumdisplay,SLOT(setFFTData(QVector<double>)));
    connect(audiomskdemodulator, SIGNAL(Plottables(double,double,double)),              ui->spectrumdisplay,SLOT(setPlottables(double,double,double)));
    connect(audiomskdemodulator, SIGNAL(SampleRateChanged(double)),                     ui->spectrumdisplay,SLOT(setSampleRate(double)));
    connect(audiomskdemodulator, SIGNAL(ScatterPoints(QVector<cpx_type>)),              ui->scatterplot,SLOT(setData(QVector<cpx_type>)));
    connect(ui->spectrumdisplay, SIGNAL(CenterFreqChanged(double)),                     audiomskdemodulator,SLOT(CenterFreqChangedSlot(double)));
    connect(ui->action_About,    SIGNAL(triggered()),                                   this, SLOT(AboutSlot()));
    connect(audiomskdemodulator, SIGNAL(BitRateChanged(double)),                        aerol,SLOT(setBitRate(double)));

//aeroL human info text
    connect(aerol,SIGNAL(HumanReadableInformation(QString)),ui->inputwidget,SLOT(appendPlainText(QString)));
    connect(aerol,SIGNAL(DataCarrierDetect(bool)),this,SLOT(DataCarrierDetectStatusSlot(bool)));


    //load settings
    QSettings settings("Jontisoft", "JAEROL");
    ui->comboBoxafc->setCurrentIndex(settings.value("comboBoxafc",0).toInt());
    ui->comboBoxbps->setCurrentIndex(settings.value("comboBoxbps",0).toInt());
    ui->comboBoxlbw->setCurrentIndex(settings.value("comboBoxlbw",0).toInt());
    ui->comboBoxDisplayformat->setCurrentIndex(settings.value("comboBoxDisplayformat",0).toInt());
    ui->comboBoxdisplay->setCurrentIndex(settings.value("comboBoxdisplay",0).toInt());
    ui->actionConnectToUDPPort->setChecked(settings.value("actionConnectToUDPPort",false).toBool());
    ui->actionRawOutput->setChecked(settings.value("actionRawOutput",false).toBool());
    double tmpfreq=settings.value("freq_center",1000).toDouble();
    ui->inputwidget->setPlainText(settings.value("inputwidget","").toString());

    //set audio msk demodulator settings and start
    on_comboBoxafc_currentIndexChanged(ui->comboBoxafc->currentText());
    on_comboBoxbps_currentIndexChanged(ui->comboBoxbps->currentText());
    on_comboBoxlbw_currentIndexChanged(ui->comboBoxlbw->currentText());
    on_comboBoxdisplay_currentIndexChanged(ui->comboBoxdisplay->currentText());
    on_actionConnectToUDPPort_toggled(ui->actionConnectToUDPPort->isChecked());

    audiomskdemodulatorsettings.freq_center=tmpfreq;
    audiomskdemodulator->setSQL(false);
    audiomskdemodulator->setSettings(audiomskdemodulatorsettings);
    audiomskdemodulator->start();

//start modulator setup. the modulator is new and is and add on.

    //connect the encoder to the text widget. the text encoder to the modulator gets done later
    varicodepipeencoder->ConnectSourceDevice(ui->inputwidget->textinputdevice);

    //always connected
    //connect(ui->actionTXRX,SIGNAL(triggered(bool)),ui->inputwidget,SLOT(reset()));//not needed
    connect(ui->actionClearTXWindow,SIGNAL(triggered(bool)),ui->inputwidget,SLOT(clear()));

    //set modulator settings and connections

    settingsdialog->populatesettings();


    //set audio msk modulator settings
    audiomskmodulatorsettings.freq_center=settingsdialog->audiomskmodulatorsettings.freq_center;//this is the only one that gets set
    audiomskmodulatorsettings.secondsbeforereadysignalemited=settingsdialog->audiomskmodulatorsettings.secondsbeforereadysignalemited;//well there is two now
    ui->inputwidget->textinputdevice->preamble1=settingsdialog->Preamble1;
    ui->inputwidget->textinputdevice->preamble2=settingsdialog->Preamble2;
    ui->inputwidget->textinputdevice->postamble=settingsdialog->Postamble;
    ui->inputwidget->reset();
    audiomskmodulator->setSettings(audiomskmodulatorsettings);

    //connections for audio out
    audiomskmodulator->ConnectSourceDevice(varicodepipeencoder);
    connect(ui->actionTXRX,SIGNAL(triggered(bool)),audiomskmodulator,SLOT(startstop(bool)));
    connect(ui->inputwidget->textinputdevice,SIGNAL(eof()),audiomskmodulator,SLOT(stopgraceful()));
    connect(audiomskmodulator,SIGNAL(statechanged(bool)),ui->actionTXRX,SLOT(setChecked(bool)));
    connect(audiomskmodulator,SIGNAL(statechanged(bool)),ui->inputwidget,SLOT(reset()));
    connect(audiomskmodulator,SIGNAL(ReadyState(bool)),ui->inputwidget->textinputdevice,SLOT(SinkReadySlot(bool)));//connection to signal textinputdevice to go from preamble to normal operation


//--end modulator setup

    //add todays date
    ui->inputwidget->appendHtml("<b>"+QDateTime::currentDateTime().toString("h:mmap ddd d-MMM-yyyy")+" JAREOL started</b>");
    QTimer::singleShot(100,ui->inputwidget,SLOT(scrolltoend()));
}

MainWindow::~MainWindow()
{

    //save settings
    QSettings settings("Jontisoft", "JAEROL");
    settings.setValue("comboBoxafc", ui->comboBoxafc->currentIndex());
    settings.setValue("comboBoxbps", ui->comboBoxbps->currentIndex());
    settings.setValue("comboBoxlbw", ui->comboBoxlbw->currentIndex());
    settings.setValue("comboBoxdisplay", ui->comboBoxdisplay->currentIndex());
    settings.setValue("comboBoxDisplayformat", ui->comboBoxDisplayformat->currentIndex());
    settings.setValue("actionConnectToUDPPort", ui->actionConnectToUDPPort->isChecked());
    settings.setValue("actionRawOutput", ui->actionRawOutput->isChecked());
    settings.setValue("freq_center", audiomskdemodulator->getCurrentFreq());
    settings.setValue("inputwidget", ui->inputwidget->toPlainText());

    delete ui;
}

void MainWindow::SignalStatusSlot(bool signal)
{
    if(signal)ui->ledsignal->setLED(QIcon::On);
     else {ui->ledsignal->setLED(QIcon::Off);aerol->LostSignal();}
}

void MainWindow::DataCarrierDetectStatusSlot(bool dcd)
{
    if(dcd)ui->leddata->setLED(QIcon::On);
     else ui->leddata->setLED(QIcon::Off);
}

void MainWindow::EbNoSlot(double EbNo)
{
    ebnolabel->setText(((QString)" EbNo: %1dB ").arg((int)round(EbNo),2, 10, QChar('0')));
}

void MainWindow::WarningTextSlot(QString warning)
{
    ui->statusBar->showMessage("Warning: "+warning,5000);
}

void MainWindow::PeakVolumeSlot(double Volume)
{
    if(Volume>0.9)ui->ledvolume->setLED(QIcon::On,QIcon::Disabled);
     else if(Volume<0.1)ui->ledvolume->setLED(QIcon::Off);
      else ui->ledvolume->setLED(QIcon::On);
}

void MainWindow::PlottablesSlot(double freq_est,double freq_center,double bandwidth)
{
    Q_UNUSED(freq_center);
    Q_UNUSED(bandwidth);
    QString str=(((QString)"%1Hz  ").arg(freq_est,0, 'f', 2)).rightJustified(11,' ');
    freqlabel->setText("  Freq: "+str);
}

void MainWindow::AboutSlot()
{
    QMessageBox::about(this,"JAEROL",""
                                     "<H1>An Aero-L demodulator and decoder</H1>"
                                     "<H3>v1.0.0</H3>"
                                     "<p>This is a program to demodulate and decode Aero-L P signals. This Aeronautical (Classic Aero) from Inmarsat using low gain antennas.</p>"
                                     "<p>For more information about this application see who knows just yet<!--<a href=\"http://jontio.zapto.org/hda1/jmsk.html\">http://jontio.zapto.org/hda1/jmsk.html</a>-->.</p>"
                                     "<p>Jonti 2015</p>" );
}

void MainWindow::on_comboBoxbps_currentIndexChanged(const QString &arg)
{
    QString arg1=arg;
    if(arg1.isEmpty())
    {
        ui->comboBoxbps->setCurrentIndex(0);
        arg1=ui->comboBoxbps->currentText();
    }

    //demodulator settings
    audiomskdemodulatorsettings.fb=arg1.split(" ")[0].toDouble();
    if(audiomskdemodulatorsettings.fb==600){audiomskdemodulatorsettings.symbolspercycle=12;audiomskdemodulatorsettings.Fs=12000;}
    if(audiomskdemodulatorsettings.fb==1200){audiomskdemodulatorsettings.symbolspercycle=12;audiomskdemodulatorsettings.Fs=24000;}

    if(audiomskdemodulatorsettings.fb==600){audiomskdemodulatorsettings.symbolspercycle=12;audiomskdemodulatorsettings.Fs=48000;}
    if(audiomskdemodulatorsettings.fb==1200){audiomskdemodulatorsettings.symbolspercycle=12;audiomskdemodulatorsettings.Fs=48000;}


    int idx=ui->comboBoxlbw->findText(((QString)"%1 Hz").arg(audiomskdemodulatorsettings.fb*1.5));
    if(idx>=0)ui->comboBoxlbw->setCurrentIndex(idx);
    audiomskdemodulatorsettings.freq_center=audiomskdemodulator->getCurrentFreq();
    audiomskdemodulator->setSettings(audiomskdemodulatorsettings);

    //audio modulator setting
    audiomskmodulatorsettings.fb=audiomskdemodulatorsettings.fb;
    audiomskmodulatorsettings.Fs=audiomskdemodulatorsettings.Fs;
    audiomskmodulator->setSettings(audiomskmodulatorsettings);


}

void MainWindow::on_comboBoxlbw_currentIndexChanged(const QString &arg1)
{
    audiomskdemodulatorsettings.lockingbw=arg1.split(" ")[0].toDouble();
    audiomskdemodulatorsettings.freq_center=audiomskdemodulator->getCurrentFreq();
    audiomskdemodulator->setSettings(audiomskdemodulatorsettings);
}

void MainWindow::on_comboBoxafc_currentIndexChanged(const QString &arg1)
{
    if(arg1=="AFC on")audiomskdemodulator->setAFC(true);
     else audiomskdemodulator->setAFC(false);
}

void MainWindow::on_comboBoxDisplayformat_currentIndexChanged(const QString &arg1)
{
    if(arg1=="1")aerol->setCompactHumanReadableInformationMode(1);
    if(arg1=="2")aerol->setCompactHumanReadableInformationMode(2);
}

void MainWindow::on_actionCleanConsole_triggered()
{
    ui->console->clear();
}

void MainWindow::on_comboBoxdisplay_currentIndexChanged(const QString &arg1)
{
    ui->scatterplot->graph(0)->clearData();
    ui->scatterplot->replot();
    ui->scatterplot->setDisksize(3);
    if(arg1=="Constellation")audiomskdemodulator->setScatterPointType(MskDemodulator::SPT_constellation);
    if(arg1=="Symbol phase"){audiomskdemodulator->setScatterPointType(MskDemodulator::SPT_phaseoffsetest);ui->scatterplot->setDisksize(6);}
    if(arg1=="None")audiomskdemodulator->setScatterPointType(MskDemodulator::SPT_None);
}

void MainWindow::on_actionConnectToUDPPort_toggled(bool arg1)
{
    audiomskdemodulator->DisconnectSinkDevice();
    aerol->DisconnectSinkDevice();
    udpsocket->close();
    if(arg1)
    {
        ui->actionRawOutput->setEnabled(true);
        ui->console->setEnableUpdates(false,"Console disabled while raw demodulated data is routed to UDP port 8765 at LocalHost.");
        udpsocket->connectToHost(QHostAddress::LocalHost, 8765);

        if(ui->actionRawOutput->isChecked())
        {
            audiomskdemodulator->ConnectSinkDevice(udpsocket);
            ui->console->setEnableUpdates(false,"Console disabled while raw demodulated data is routed to UDP port 8765 at LocalHost.");
        }
         else
         {
            audiomskdemodulator->ConnectSinkDevice(aerol);
            aerol->ConnectSinkDevice(udpsocket);
            ui->console->setEnableUpdates(false,"Console disabled while decoded and demodulated data is routed to UDP port 8765 at LocalHost.");
         }

    }
     else
     {
         audiomskdemodulator->ConnectSinkDevice(aerol);
         aerol->ConnectSinkDevice(ui->console->consoledevice);
         ui->console->setEnableUpdates(true);
     }
}

void MainWindow::on_actionRawOutput_triggered()
{
    on_actionConnectToUDPPort_toggled(ui->actionConnectToUDPPort->isChecked());
}

void MainWindow::on_action_Settings_triggered()
{
    settingsdialog->populatesettings();
    if(settingsdialog->exec()==QDialog::Accepted)
    {

        //it a tad to tricky keeping the modulator running when settings have been changed so i'm tring out stopping the modulator when settings are changed
        audiomskmodulator->stop();

        ui->statusBar->clearMessage();
        ui->inputwidget->textinputdevice->preamble1=settingsdialog->Preamble1;
        ui->inputwidget->textinputdevice->preamble2=settingsdialog->Preamble2;
        ui->inputwidget->textinputdevice->postamble=settingsdialog->Postamble;

        audiomskmodulatorsettings.freq_center=settingsdialog->audiomskmodulatorsettings.freq_center;//this is the only one that gets set
        audiomskmodulatorsettings.secondsbeforereadysignalemited=settingsdialog->audiomskmodulatorsettings.secondsbeforereadysignalemited;//well there is two now
        audiomskmodulator->setSettings(audiomskmodulatorsettings);     

    }
}




