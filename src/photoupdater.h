#ifndef PHOTOUPDATER_H
#define PHOTOUPDATER_H

namespace PhotoUpdater {
void start();
void check();
bool configured();
bool automaticallyChecks();
void setAutomaticallyChecks(bool enabled);
}

#endif
