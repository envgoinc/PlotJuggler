#ifndef STATE_PUBLISHER_ZMQ_H
#define STATE_PUBLISHER_ZMQ_H

#include <QObject>
#include <QtPlugin>
#include <QDialog>
#include <QTimer>
#include <thread>
#include <mutex>
#include <vector>
#include "ui_publisher_csv_dialog.h"
#include "PlotJuggler/statepublisher_base.h"

class QTextStream;

class StatePublisherCSV : public PJ::StatePublisher
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.StatePublisher")
  Q_INTERFACES(PJ::StatePublisher)

public:
  StatePublisherCSV();

  virtual const char* name() const override
  {
    return "CSV Exporter";
  }

  virtual ~StatePublisherCSV() override;

  virtual bool enabled() const override
  {
    return _enabled;
  }

  virtual void updateState(double current_time) override
  {
    _previous_time = current_time;
  }

  virtual void play(double) override
  {
  }

public slots:
  virtual void setEnabled(bool enabled) override;

private slots:
  void onWindowClosed();

private:
  std::mutex _mutex;

  bool _enabled = false;
  double _previous_time = 0;
  double _start_time = std::numeric_limits<double>::quiet_NaN();
  double _end_time = std::numeric_limits<double>::quiet_NaN();

  QDialog* _dialog;
  Ui::PublisherCSV_DIalog* _ui;

  QTimer* _notification_timer;

  void delayedClearNotification();

  bool exportOnlyPlotted() const;

  std::vector<std::pair<std::string, const PJ::PlotData*>> selectedNumericPlots(
      double time_start, double time_end) const;

  QString generateRangeCSV(double time_start, double time_end);

  QString generateStatisticsCSV(double time_start, double time_end);

  bool getTimeRanges(double* first, double* last);

  void updateButtonsState();

  QString promptFileName() const;

  void saveFile(const QString& text);

  bool writeStatisticsCSV(QTextStream& stream, double time_start, double time_end);

  bool writeRangeCSV(QTextStream& stream, double time_start, double time_end);
};

#endif  // STATE_PUBLISHER_ZMQ_H
