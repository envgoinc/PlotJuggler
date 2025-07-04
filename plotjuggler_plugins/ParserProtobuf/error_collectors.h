#ifndef ERROR_COLLECTORS_H
#define ERROR_COLLECTORS_H

#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/compiler/importer.h>

#include <QStringList>

class IoErrorCollector : public google::protobuf::io::ErrorCollector
{
public:
  void AddError(int line, google::protobuf::io::ColumnNumber column,
                const std::string& message);

  void AddWarning(int line, google::protobuf::io::ColumnNumber column,
                  const std::string& message);

  const QStringList& errors()
  {
    return _errors;
  }

private:
  QStringList _errors;
};

class FileErrorCollector : public google::protobuf::compiler::MultiFileErrorCollector
{
public:
  void AddError(const std::string& filename, int line, int,
                const std::string& message);

  void AddWarning(const std::string& filename, int line, int,
                  const std::string& message);

  const QStringList& errors()
  {
    return _errors;
  }

  void RecordError(absl::string_view filename, int line, int column,
                   absl::string_view message) override
  {

  }

private:
  QStringList _errors;
};

#endif  // ERROR_COLLECTORS_H
