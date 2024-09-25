#ifndef COMPOSITEWIDGET_H
#define COMPOSITEWIDGET_H

#include <QWidget>

class Collapsable
{
public:
	virtual bool collapsed() = 0;
	virtual void setCollapsed(bool b) = 0;
};

class CompositeWidget
{
public:
	virtual void add(QWidget *w) = 0;
	virtual void remove(QWidget *w) = 0;
};

#endif // COMPOSITEWIDGET_H