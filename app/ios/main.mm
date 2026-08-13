#import <UIKit/UIKit.h>

#include "core/cpu/arm64_jit.h"
#include "core/cpu/x86_decoder.h"

@interface VSHiftJITViewController : UIViewController
@end

@implementation VSHiftJITViewController
- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];

    constexpr std::uint8_t program[] = {
        0xB8, 0x28, 0x00, 0x00, 0x00,
        0x83, 0xC0, 0x02,
        0xC3,
    };
    const auto decoded = vshift::cpu::Decode(program);
    const auto compiled = vshift::cpu::Arm64Jit::Compile(decoded.instructions);

    std::uint32_t result = 0;
    const bool executed = compiled.ok() && compiled.jit->Execute(result);
    NSString *message = nil;
    if (executed) {
        message = [NSString stringWithFormat:@"ARM64 JIT result: %u", result];
    } else if (!compiled.ok()) {
        message = [NSString stringWithFormat:
            @"JIT memory setup failed: %@\nCheck signing/JIT entitlement.",
            [NSString stringWithUTF8String:compiled.error.c_str()]];
    } else {
        message = @"JIT compiled but did not execute on this architecture.";
    }

    self.view.backgroundColor = UIColor.systemBackgroundColor;
    UILabel *label = [[UILabel alloc] initWithFrame:self.view.bounds];
    label.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    label.textAlignment = NSTextAlignmentCenter;
    label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
    label.numberOfLines = 0;
    label.text = message;
    [self.view addSubview:label];
}
@end

@interface VSHiftAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

@implementation VSHiftAppDelegate
- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    (void)application;
    (void)launchOptions;
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[VSHiftJITViewController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil,
                                 NSStringFromClass([VSHiftAppDelegate class]));
    }
}
