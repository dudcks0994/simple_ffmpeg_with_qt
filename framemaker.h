#ifndef FRAMEMAKER_H
#define FRAMEMAKER_H

#include <QObject>

class FrameMaker : public QObject
{
    Q_OBJECT
public:
    explicit FrameMaker(QObject *parent = nullptr);

signals:
};

#endif // FRAMEMAKER_H
