#include "photoupdater.h"
#include "nestbackgroundactivity.h"
#import <AppKit/AppKit.h>
#import <Sparkle/Sparkle.h>
#include <QApplication>
#include <QProgressDialog>

static bool hasActiveWork()
{
    if (NestBackgroundActivity::active()) return true;
    if (QApplication::activeModalWidget()) return true;
    for (QWidget *widget : QApplication::topLevelWidgets())
        if (qobject_cast<QProgressDialog *>(widget) && widget->isVisible()) return true;
    return false;
}

@interface NestUpdateDelegate : NSObject <SPUUpdaterDelegate>
@property(strong) NSTimer *waitTimer;
@end

@implementation NestUpdateDelegate
- (BOOL)updater:(SPUUpdater *)updater shouldPostponeRelaunchForUpdate:(SUAppcastItem *)item
    untilInvokingBlock:(void (^)(void))installHandler
{
    if (!hasActiveWork()) return NO;
    [self.waitTimer invalidate];
    __weak NestUpdateDelegate *weakSelf = self;
    self.waitTimer = [NSTimer timerWithTimeInterval:0.5 repeats:YES block:^(NSTimer *timer) {
        if (!weakSelf) { [timer invalidate]; return; }
        if (hasActiveWork()) return;
        [timer invalidate];
        weakSelf.waitTimer = nil;
        installHandler();
    }];
    [NSRunLoop.mainRunLoop addTimer:self.waitTimer forMode:NSRunLoopCommonModes];
    return YES;
}
@end

@interface NestUpdateDriver : SPUStandardUserDriver
@end
@implementation NestUpdateDriver
- (void)showUpdateFoundWithAppcastItem:(SUAppcastItem *)item state:(SPUUserUpdateState *)state reply:(void (^)(SPUUserUpdateChoice))reply
{
    if (item.informationOnlyUpdate || state.stage != SPUUserUpdateStageNotDownloaded) {
        [super showUpdateFoundWithAppcastItem:item state:state reply:reply];
        return;
    }
    [super dismissUpdateInstallation];
    NSAlert *choice = [[NSAlert alloc] init];
    choice.messageText = @"새 업데이트가 있습니다";
    NSString *version = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    choice.informativeText = [NSString stringWithFormat:
        @"현재 버전: %@ → 새 버전: %@\n\n앱에서 설치하거나 DMG를 직접 다운로드할 수 있습니다.", version ?: @"", item.displayVersionString];
    [choice addButtonWithTitle:@"자동 업데이트"];
    [choice addButtonWithTitle:@"설치 파일 다운로드"];
    [choice addButtonWithTitle:@"취소"];
    NSModalResponse response = [choice runModal];
    if (response == NSAlertFirstButtonReturn) {
        reply(SPUUserUpdateChoiceInstall);
    } else {
        reply(SPUUserUpdateChoiceDismiss);
        if (response != NSAlertSecondButtonReturn) return;
        NSString *address = [NSBundle.mainBundle objectForInfoDictionaryKey:@"NVManualDownloadURL"];
        NSURL *url = [NSURL URLWithString:address ?: @""];
        BOOL valid = [url.scheme isEqualToString:@"https"] && [url.host isEqualToString:@"github.com"]
            && [url.path hasSuffix:@"/releases/latest/download/NestView-macOS-AppleSilicon.dmg"];
        if (!valid || ![NSWorkspace.sharedWorkspace openURL:url]) {
            NSAlert *error = [[NSAlert alloc] init];
            error.messageText = @"다운로드 주소를 열 수 없습니다";
            error.informativeText = @"기본 브라우저 설정과 배포용 앱의 다운로드 주소를 확인해 주세요.";
            [error addButtonWithTitle:@"확인"];
            [error runModal];
        }
    }
}
@end

namespace {
SPUUpdater *controller;
NestUpdateDriver *userDriver;
NestUpdateDelegate *updateDelegate;
NSError *startupError;

bool hasConfiguration()
{
    NSBundle *bundle = NSBundle.mainBundle;
    if (![bundle.bundleURL.pathExtension isEqualToString:@"app"]) return false;
    NSString *feed = [bundle objectForInfoDictionaryKey:@"SUFeedURL"];
    NSString *key = [bundle objectForInfoDictionaryKey:@"SUPublicEDKey"];
    NSURL *url = [NSURL URLWithString:feed ?: @""];
    return [url.scheme isEqualToString:@"https"] && [url.host isEqualToString:@"github.com"]
        && [[NSData alloc] initWithBase64EncodedString:key ?: @"" options:0].length == 32;
}
}

namespace PhotoUpdater {
bool configured() { return hasConfiguration(); }

void start()
{
    if (controller || !hasConfiguration()) return;
    updateDelegate = [[NestUpdateDelegate alloc] init];
    userDriver = [[NestUpdateDriver alloc] initWithHostBundle:NSBundle.mainBundle delegate:nil];
    controller = [[SPUUpdater alloc] initWithHostBundle:NSBundle.mainBundle applicationBundle:NSBundle.mainBundle
        userDriver:userDriver delegate:updateDelegate];
    NSError *error = nil;
    if (![controller startUpdater:&error]) {
        startupError = error;
        controller = nil;
        userDriver = nil;
    } else startupError = nil;
}

void check()
{
    if (!hasConfiguration()) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"업데이트 서버가 설정되지 않았습니다";
        alert.informativeText = @"이 빌드에는 배포 저장소가 설정되지 않았습니다. 배포용 NestView 앱에서 다시 확인해 주세요.";
        [alert addButtonWithTitle:@"확인"];
        [alert runModal];
        return;
    }
    start();
    if (controller) {
        [controller checkForUpdates];
    } else {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"업데이트 확인을 시작할 수 없습니다";
        alert.informativeText = startupError.localizedDescription ?: @"배포용 앱을 다시 설치한 뒤 확인해 주세요.";
        [alert addButtonWithTitle:@"확인"];
        [alert runModal];
    }
}

bool automaticallyChecks()
{
    start();
    return controller && controller.automaticallyChecksForUpdates;
}

void setAutomaticallyChecks(bool enabled)
{
    start();
    if (controller) controller.automaticallyChecksForUpdates = enabled;
}
}
