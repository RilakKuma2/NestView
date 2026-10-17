#pragma once
#include <QPoint>
#include <Qt>

// Use input timestamps, not decoding/GUI processing time. Backlogged wheel
// events must not turn into extra page turns on a slow image or CI machine.
class NestTrackpadNavigation {
public:
    int step(QPoint delta, Qt::ScrollPhase phase, quint64 timestamp) {
        if (phase == Qt::ScrollMomentum) return 0;
        if (phase == Qt::ScrollEnd) { reset(); return 0; }
        const int movement = qAbs(delta.x()) > qAbs(delta.y()) ? delta.x() : delta.y();
        const int nextDirection = movement > 0 ? 1 : movement < 0 ? -1 : 0;
        if (phase == Qt::ScrollBegin || !hasEvent || timestamp < lastEvent
            || timestamp - lastEvent > 300 || nextDirection * direction < 0) reset();
        hasEvent = true;
        lastEvent = timestamp;
        if (nextDirection) direction = nextDirection;
        if (hasStep && timestamp - lastStep < 100) { travel = {}; return 0; }
        travel += delta;
        const int amount = qAbs(travel.x()) > qAbs(travel.y()) ? travel.x() : travel.y();
        if (qAbs(amount) < 12) return 0;
        travel = {};
        lastStep = timestamp;
        hasStep = true;
        return amount > 0 ? -1 : 1;
    }
    void reset() { travel = {}; direction = 0; hasStep = hasEvent = false; }
private:
    QPoint travel;
    int direction = 0;
    quint64 lastEvent = 0, lastStep = 0;
    bool hasEvent = false, hasStep = false;
};
