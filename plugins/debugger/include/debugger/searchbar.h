#ifndef SCOPY_SEARCHBAR_H
#define SCOPY_SEARCHBAR_H

#include <QWidget>
#include <QLineEdit>
#include <QCompleter>
#include <QStandardItemModel>
#include <QPushButton>
#include <QLabel>
#include <QSet>

namespace scopy::debugger {
class SearchBar : public QWidget
{
	Q_OBJECT
public:
	explicit SearchBar(QSet<QString> options, QWidget *parent = nullptr);

	QLineEdit *getLineEdit();

private:
	QLabel *m_label;
	QLineEdit *m_lineEdit;
	QCompleter *m_completer;
};
} // namespace scopy::debugger

#endif // SCOPY_SEARCHBAR_H