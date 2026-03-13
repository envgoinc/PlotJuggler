#include <QFile>
#include <QMessageBox>
#include <thread>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QSettings>
#include <QTextStream>
#include "publisher_csv.h"

StatePublisherCSV::StatePublisherCSV()
{
}

StatePublisherCSV::~StatePublisherCSV()
{
}

void StatePublisherCSV::setEnabled(bool enabled)
{
  _enabled = enabled;

  if (_enabled)
  {
    _dialog = new QDialog();
    _notification_timer = new QTimer(_dialog);
    _notification_timer->setSingleShot(true);

    _ui = new Ui::PublisherCSV_DIalog();
    _ui->setupUi(_dialog);
    _dialog->setAttribute(Qt::WA_DeleteOnClose);

    _start_time = std::numeric_limits<double>::quiet_NaN();
    _end_time = std::numeric_limits<double>::quiet_NaN();
    _ui->lineEditStart->setText("");
    _ui->lineEditEnd->setText("");

    updateButtonsState();

    //--------------------
    connect(_notification_timer, &QTimer::timeout, this,
            [this]() { _ui->labelNotification->setText(""); });
    //--------------------
    connect(_dialog, &QDialog::finished, this, &StatePublisherCSV::onWindowClosed);

    //--------------------
    connect(_ui->buttonGetStart, &QPushButton::clicked, this, [this]() {
      _start_time = _previous_time;
      _ui->checkBoxFirst->setChecked(false);
      _ui->lineEditStart->setEnabled(true);
      _ui->lineEditStart->setText(QString::number(_previous_time, 'f', 3));
      updateButtonsState();
    });
    //--------------------
    connect(_ui->buttonGetEnd, &QPushButton::clicked, this, [this]() {
      _end_time = _previous_time;
      _ui->checkBoxLast->setChecked(false);
      _ui->lineEditEnd->setEnabled(true);
      _ui->lineEditEnd->setText(QString::number(_previous_time, 'f', 3));
      updateButtonsState();
    });
    //--------------------
    connect(_ui->checkBoxFirst, &QCheckBox::toggled, this, [this](bool checked) {
      _ui->lineEditStart->setEnabled(!checked);
      _start_time = (checked) ? std::numeric_limits<double>::lowest() : _previous_time;
      updateButtonsState();
    });
    //--------------------
    connect(_ui->checkBoxLast, &QCheckBox::toggled, this, [this](bool checked) {
      _ui->lineEditEnd->setEnabled(!checked);
      _end_time = (checked) ? std::numeric_limits<double>::max() : _previous_time;

      updateButtonsState();
    });
    //--------------------
    connect(_ui->buttonStatisticsClip, &QPushButton::clicked, this, [this]() {
      auto csv_string = generateStatisticsCSV(_start_time, _end_time);
      QClipboard* clipboard = QGuiApplication::clipboard();
      clipboard->setText(csv_string);
      _ui->labelNotification->setText("Statistics copied to Clipboard");
      _notification_timer->start(2000);
    });

    //--------------------
    connect(_ui->buttonRangeClip, &QPushButton::clicked, this, [this]() {
      auto csv_string = generateRangeCSV(_start_time, _end_time);
      QClipboard* clipboard = QGuiApplication::clipboard();
      clipboard->setText(csv_string);
      _ui->labelNotification->setText("Range data copied to Clipboard");
      _notification_timer->start(2000);
    });

    //--------------------
    connect(_ui->buttonStatisticsFile, &QPushButton::clicked, this, [this]() {
      QString fileName = promptFileName();
      if (fileName.isEmpty())
      {
        return;
      }

      QFile file(fileName);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
      {
        QMessageBox::warning(nullptr, "Error",
                             QString("Failed to open the file [%1]").arg(fileName));
        return;
      }

      QTextStream stream(&file);
      if (!writeStatisticsCSV(stream, _start_time, _end_time))
      {
        return;
      }

      file.close();

      QSettings settings;
      settings.setValue("StatePublisherCSV.saveDirectory", QFileInfo(fileName).absolutePath());
    });

    //--------------------
    connect(_ui->buttonRangeFile, &QPushButton::clicked, this, [this]() {
      QString fileName = promptFileName();
      if (fileName.isEmpty())
      {
        return;
      }

      QFile file(fileName);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
      {
        QMessageBox::warning(nullptr, "Error",
                             QString("Failed to open the file [%1]").arg(fileName));
        return;
      }

      QTextStream stream(&file);
      if (!writeRangeCSV(stream, _start_time, _end_time))
      {
        return;
      }

      file.close();

      QSettings settings;
      settings.setValue("StatePublisherCSV.saveDirectory", QFileInfo(fileName).absolutePath());
    });

    //--------------------
    _dialog->setWindowFlag(Qt::WindowStaysOnTopHint);
    _dialog->show();
  }
  else
  {
    if (_dialog)
    {
      _dialog->done(0);
    }
  }
}

void StatePublisherCSV::onWindowClosed()
{
  _enabled = false;
  emit closed();
}

QString StatePublisherCSV::generateStatisticsCSV(double time_start, double time_end)
{
  QString output;
  QTextStream stream(&output);
  writeStatisticsCSV(stream, time_start, time_end);
  return output;
}

bool StatePublisherCSV::exportOnlyPlotted() const
{
  return _ui && _ui->checkBoxOnlyPlotted->isChecked();
}

std::vector<std::pair<std::string, const PJ::PlotData*>> StatePublisherCSV::selectedNumericPlots(
    double time_start, double time_end) const
{
  std::vector<std::pair<std::string, const PJ::PlotData*>> selected_plots;
  std::unordered_set<std::string> visible_curves;

  if (exportOnlyPlotted() && _visible_curves_provider)
  {
    visible_curves = _visible_curves_provider();
  }

  for (const auto& it : _datamap->numeric)
  {
    if (exportOnlyPlotted() && visible_curves.count(it.first) == 0)
    {
      continue;
    }
    if (it.second.size() == 0 || it.second.front().x > time_end || it.second.back().x < time_start)
    {
      continue;
    }
    selected_plots.push_back({ it.first, &it.second });
  }

  std::sort(selected_plots.begin(), selected_plots.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  return selected_plots;
}

bool StatePublisherCSV::writeStatisticsCSV(QTextStream& out, double time_start, double time_end)
{
  const auto ordered_plots = selectedNumericPlots(time_start, time_end);

  out << "Series,Current,Min,Max,Average\n";
  out << "Start Time," << time_start << "\n";
  out << "End Time," << time_end << "\n";
  out << "Current Time," << _previous_time << "\n";

  for (const auto& it : ordered_plots)
  {
    const auto& name = it.first;
    const auto& plot = *(it.second);
    int index = plot.getIndexFromX(time_start);
    if (index < 0)
    {
      continue;  // skip this
    }

    auto current_value = plot.getYfromX(_previous_time);

    auto point = plot.at(index);

    if (point.x > time_end || index + 1 == plot.size())  // out of range
    {
      continue;
    }

    double min_value = point.y;
    double max_value = point.y;
    double total = point.y;
    int count = 1;
    index++;

    while (index < plot.size())
    {
      point = plot.at(index);
      if (point.x > time_end)
      {
        break;
      }
      double value = point.y;
      max_value = std::max(max_value, value);
      min_value = std::min(min_value, value);
      total += value;
      count++;
      index++;
    }
    out << QString::fromStdString(name) << ',';
    if (current_value)
    {
      out << QString::number(current_value.value());
    }
    out << ',';
    out << QString::number(min_value) << ',';
    out << QString::number(max_value) << ',';
    out << QString::number(total / double(count)) << '\n';
  }
  return (out.status() == QTextStream::Ok);
}

bool StatePublisherCSV::getTimeRanges(double* first, double* last)
{
  bool ok;
  *first = _ui->lineEditStart->text().toDouble(&ok);
  if (!ok)
  {
    return false;
  }
  *last = _ui->lineEditEnd->text().toDouble(&ok);
  if (!ok)
  {
    return false;
  }
  return true;
}

void StatePublisherCSV::updateButtonsState()
{
  bool enable = (_start_time <= _end_time);
  _ui->buttonRangeClip->setEnabled(enable);
  _ui->buttonRangeFile->setEnabled(enable);
  _ui->buttonStatisticsClip->setEnabled(enable);
  _ui->buttonStatisticsFile->setEnabled(enable);
}

QString StatePublisherCSV::promptFileName() const
{
  QSettings settings;
  QString directory_path =
      settings.value("StatePublisherCSV.saveDirectory", QDir::currentPath()).toString();

  QString fileName = QFileDialog::getSaveFileName(nullptr, tr("Save as CSV file"), directory_path,
                                                  tr("CSV files (*.csv)"));

  if (fileName.isEmpty())
  {
    return QString();
  }
  if (!fileName.endsWith(".csv"))
  {
    fileName.append(".csv");
  }

  return fileName;
}

void StatePublisherCSV::saveFile(const QString& text)
{
  QString fileName = promptFileName();
  if (fileName.isEmpty())
  {
    return;
  }

  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QMessageBox::warning(nullptr, "Error", QString("Failed to open the file [%1]").arg(fileName));
    return;
  }

  QTextStream stream(&file);
  stream << text;
  file.close();

  QSettings settings;
  QString directory_path = QFileInfo(fileName).absolutePath();
  settings.setValue("StatePublisherCSV.saveDirectory", directory_path);
}

QString StatePublisherCSV::generateRangeCSV(double time_start, double time_end)
{
  QString output;
  QTextStream stream(&output);
  writeRangeCSV(stream, time_start, time_end);
  return output;
}

bool StatePublisherCSV::writeRangeCSV(QTextStream& out, double time_start, double time_end)
{
  using PlotPair = std::pair<std::string, const PJ::PlotData*>;

  std::vector<PlotPair> ordered_plotdata = selectedNumericPlots(time_start, time_end);
  const size_t plot_count = ordered_plotdata.size();

  // current index per plordata
  std::vector<size_t> indices(plot_count, 0);

  const auto NaN = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> row_values(plot_count, NaN);

  out << "__time,";
  for (size_t i = 0; i < plot_count; i++)
  {
    out << QString::fromStdString(ordered_plotdata[i].first);
    out << ((i + 1 < plot_count) ? "," : "\n");

    const PJ::PlotData* plotdata = (ordered_plotdata[i].second);
    int index = plotdata->getIndexFromX(time_start);
    if (index < 0)
    {
      index = plotdata->size();
    }
    indices[i] = index + 1;
  }

  bool done = false;

  while (!done)
  {
    // done will become false if at least one plotdata is not completed
    done = true;
    double min_time = std::numeric_limits<double>::max();

    for (size_t i = 0; i < plot_count; i++)
    {
      size_t index = indices[i];
      const PJ::PlotData* plotdata = (ordered_plotdata[i].second);
      row_values[i] = NaN;

      if (index >= plotdata->size())
      {
        continue;
      }
      const auto& point = plotdata->at(index);
      if (point.x > time_end)
      {
        continue;
      }

      done = false;

      if (min_time > point.x)
      {
        min_time = point.x;  // new min_time
        // reset previous flags
        std::fill(row_values.begin(), row_values.begin() + i, NaN);
        row_values[i] = point.y;
      }
      else if (std::abs(min_time - point.x) < std::numeric_limits<double>::epsilon())
      {
        row_values[i] = point.y;
      }
    }

    if (min_time > time_end || done)
    {
      break;
    }

    // the row to append to the CSV file
    out << QString::number(min_time, 'f', 6) << ",";

    for (size_t i = 0; i < plot_count; i++)
    {
      if (!std::isnan(row_values[i]))
      {
        out << QString::number(row_values[i], 'f', 9);
        // value used, move to the nex index
        indices[i]++;
      }
      out << ((i + 1 < plot_count) ? "," : "\n");
    }
  }
  return (out.status() == QTextStream::Ok);
}
