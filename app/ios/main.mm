#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "cores/ps3/ps3_core.h"
#include "core/firmware/ps3_package.h"
#include "core/firmware/ps3_pup.h"
#include "core/firmware/ps3_tar.h"
#include "core/loader/ps3_sce.h"
#include "core/loader/ps3_self.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>

#if defined(VSHIFT_HAVE_RPCS3_CORE)
extern "C" void vshift_rpcs3_ios_link_core();
#endif

typedef NS_ENUM(NSInteger, VSHiftPS3Screen) {
    VSHiftPS3ScreenLibrary = 0,
    VSHiftPS3ScreenStart,
    VSHiftPS3ScreenDetail,
    VSHiftPS3ScreenSettings,
};

typedef NS_ENUM(NSInteger, VSHiftButtonStyle) {
    VSHiftButtonStyleFilled = 0,
    VSHiftButtonStyleGray,
    VSHiftButtonStyleTinted,
};

@interface VSHiftDisplayViewController : UIViewController <UIGestureRecognizerDelegate>
@property(nonatomic, strong) UIImageView *framebufferView;
@property(nonatomic, strong) UIVisualEffectView *statusPill;
@property(nonatomic, strong) UILabel *displayStatusLabel;
@property(nonatomic, strong) UIActivityIndicatorView *statusActivity;
@property(nonatomic, strong) UIVisualEffectView *controlBar;
@property(nonatomic, strong) UIButton *pauseButton;
@property(nonatomic, copy) dispatch_block_t closeHandler;
@property(nonatomic, copy) dispatch_block_t powerHandler;
@property(nonatomic, copy) void (^pauseHandler)(BOOL paused);
@property(nonatomic, assign) BOOL runtimePaused;
- (void)updateFramebufferImage:(UIImage *)image;
- (void)updateDisplayStatus:(NSString *)status;
@end

@implementation VSHiftDisplayViewController

- (UIButton *)displayControlWithSymbol:(NSString *)symbol action:(SEL)action {
    UIButtonConfiguration *configuration = [UIButtonConfiguration plainButtonConfiguration];
    configuration.image = [UIImage systemImageNamed:symbol];
    configuration.preferredSymbolConfigurationForImage =
        [UIImageSymbolConfiguration configurationWithPointSize:19.0 weight:UIImageSymbolWeightMedium];
    configuration.baseForegroundColor = UIColor.whiteColor;
    UIBackgroundConfiguration *background = [UIBackgroundConfiguration clearConfiguration];
    background.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.13];
    background.cornerRadius = 22.0;
    configuration.background = background;
    configuration.contentInsets = NSDirectionalEdgeInsetsMake(0.0, 0.0, 0.0, 0.0);
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    button.configuration = configuration;
    button.translatesAutoresizingMaskIntoConstraints = NO;
    [button.widthAnchor constraintEqualToConstant:44.0].active = YES;
    [button.heightAnchor constraintEqualToConstant:44.0].active = YES;
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.blackColor;
    self.view.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;

    self.framebufferView = [[UIImageView alloc] initWithFrame:CGRectZero];
    self.framebufferView.translatesAutoresizingMaskIntoConstraints = NO;
    self.framebufferView.backgroundColor = UIColor.blackColor;
    self.framebufferView.contentMode = UIViewContentModeScaleAspectFit;
    self.framebufferView.accessibilityLabel = @"PS3 display";
    [self.view addSubview:self.framebufferView];

    self.statusPill = [[UIVisualEffectView alloc]
        initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterialDark]];
    self.statusPill.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusPill.layer.cornerRadius = 18.0;
    self.statusPill.layer.cornerCurve = kCACornerCurveContinuous;
    self.statusPill.clipsToBounds = YES;
    [self.view addSubview:self.statusPill];

    self.displayStatusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.displayStatusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.displayStatusLabel.text = @"Starting virtual console…";
    self.displayStatusLabel.textColor = UIColor.secondaryLabelColor;
    self.displayStatusLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    self.displayStatusLabel.adjustsFontForContentSizeCategory = YES;
    self.displayStatusLabel.textAlignment = NSTextAlignmentCenter;
    self.displayStatusLabel.numberOfLines = 2;
    self.displayStatusLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    self.displayStatusLabel.adjustsFontSizeToFitWidth = YES;
    [self.displayStatusLabel setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                                              forAxis:UILayoutConstraintAxisHorizontal];

    self.statusActivity = [[UIActivityIndicatorView alloc]
        initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
    self.statusActivity.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusActivity.color = UIColor.secondaryLabelColor;
    [self.statusActivity startAnimating];

    UIStackView *statusStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[self.statusActivity, self.displayStatusLabel]];
    statusStack.translatesAutoresizingMaskIntoConstraints = NO;
    statusStack.axis = UILayoutConstraintAxisHorizontal;
    statusStack.alignment = UIStackViewAlignmentCenter;
    statusStack.spacing = 9.0;
    statusStack.layoutMargins = UIEdgeInsetsMake(9.0, 14.0, 9.0, 14.0);
    statusStack.layoutMarginsRelativeArrangement = YES;
    [self.statusPill.contentView addSubview:statusStack];

    UIBlurEffect *blur = [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThinMaterialDark];
    self.controlBar = [[UIVisualEffectView alloc] initWithEffect:blur];
    self.controlBar.translatesAutoresizingMaskIntoConstraints = NO;
    self.controlBar.layer.cornerRadius = 24.0;
    self.controlBar.clipsToBounds = YES;

    UIButton *back = [self displayControlWithSymbol:@"chevron.backward" action:@selector(closeDisplay)];
    UIButton *power = [self displayControlWithSymbol:@"power" action:@selector(powerOff)];
    self.pauseButton = [self displayControlWithSymbol:@"pause.fill" action:@selector(togglePause)];
    UIButton *fit = [self displayControlWithSymbol:@"arrow.up.left.and.arrow.down.right" action:@selector(toggleDisplayFit)];
    back.accessibilityLabel = @"Back";
    power.accessibilityLabel = @"Power off";
    self.pauseButton.accessibilityLabel = @"Pause";
    fit.accessibilityLabel = @"Change display scaling";
    UIStackView *controls = [[UIStackView alloc] initWithArrangedSubviews:@[back, power, self.pauseButton, fit]];
    controls.translatesAutoresizingMaskIntoConstraints = NO;
    controls.axis = UILayoutConstraintAxisHorizontal;
    controls.alignment = UIStackViewAlignmentCenter;
    controls.spacing = 2.0;
    [self.controlBar.contentView addSubview:controls];
    [self.view addSubview:self.controlBar];

    UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(toggleControls)];
    tap.cancelsTouchesInView = NO;
    tap.delegate = self;
    [self.view addGestureRecognizer:tap];

    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [self.framebufferView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.framebufferView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.framebufferView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [self.framebufferView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [self.statusPill.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
        [self.statusPill.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
        [self.statusPill.leadingAnchor constraintGreaterThanOrEqualToAnchor:safe.leadingAnchor constant:24.0],
        [self.statusPill.trailingAnchor constraintLessThanOrEqualToAnchor:safe.trailingAnchor constant:-24.0],
        [statusStack.leadingAnchor constraintEqualToAnchor:self.statusPill.contentView.leadingAnchor],
        [statusStack.trailingAnchor constraintEqualToAnchor:self.statusPill.contentView.trailingAnchor],
        [statusStack.topAnchor constraintEqualToAnchor:self.statusPill.contentView.topAnchor],
        [statusStack.bottomAnchor constraintEqualToAnchor:self.statusPill.contentView.bottomAnchor],
        [self.controlBar.topAnchor constraintEqualToAnchor:safe.topAnchor constant:10.0],
        [self.controlBar.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-12.0],
        [controls.leadingAnchor constraintEqualToAnchor:self.controlBar.contentView.leadingAnchor constant:4.0],
        [controls.trailingAnchor constraintEqualToAnchor:self.controlBar.contentView.trailingAnchor constant:-4.0],
        [controls.topAnchor constraintEqualToAnchor:self.controlBar.contentView.topAnchor constant:2.0],
        [controls.bottomAnchor constraintEqualToAnchor:self.controlBar.contentView.bottomAnchor constant:-2.0],
    ]];
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    if (@available(iOS 16.0, *)) {
        UIWindowScene *scene = self.view.window.windowScene;
        UIWindowSceneGeometryPreferencesIOS *preferences =
            [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscape];
        [scene requestGeometryUpdateWithPreferences:preferences errorHandler:^(NSError *error) {
            (void)error;
        }];
        [self setNeedsUpdateOfSupportedInterfaceOrientations];
    }
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)prefersHomeIndicatorAutoHidden {
    return YES;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {
    return UIRectEdgeAll;
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (void)closeDisplay {
    if (self.closeHandler != nil) self.closeHandler();
    UIViewController *presenting = self.presentingViewController;
    [self dismissViewControllerAnimated:YES completion:^{
        if (@available(iOS 16.0, *)) {
            UIWindowScene *scene = presenting.view.window.windowScene;
            UIWindowSceneGeometryPreferencesIOS *preferences =
                [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:UIInterfaceOrientationMaskPortrait];
            [scene requestGeometryUpdateWithPreferences:preferences errorHandler:^(NSError *error) {
                (void)error;
            }];
            [presenting setNeedsUpdateOfSupportedInterfaceOrientations];
        }
    }];
}

- (void)powerOff {
    if (self.powerHandler != nil) self.powerHandler();
    [self closeDisplay];
}

- (void)togglePause {
    self.runtimePaused = !self.runtimePaused;
    UIButtonConfiguration *configuration = self.pauseButton.configuration;
    configuration.image = [UIImage systemImageNamed:self.runtimePaused ? @"play.fill" : @"pause.fill"];
    self.pauseButton.configuration = configuration;
    if (self.pauseHandler != nil) self.pauseHandler(self.runtimePaused);
}

- (void)toggleDisplayFit {
    self.framebufferView.contentMode = self.framebufferView.contentMode == UIViewContentModeScaleAspectFit
        ? UIViewContentModeScaleAspectFill
        : UIViewContentModeScaleAspectFit;
}

- (void)toggleControls {
    [UIView animateWithDuration:0.2 animations:^{
        self.controlBar.alpha = self.controlBar.alpha > 0.5 ? 0.0 : 1.0;
    }];
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch {
    (void)gestureRecognizer;
    return ![touch.view isDescendantOfView:self.controlBar];
}

- (void)updateFramebufferImage:(UIImage *)image {
    dispatch_async(dispatch_get_main_queue(), ^{
        self.framebufferView.image = image;
        self.statusPill.hidden = image != nil;
        if (image != nil) [self.statusActivity stopAnimating];
    });
}

- (void)updateDisplayStatus:(NSString *)status {
    dispatch_async(dispatch_get_main_queue(), ^{
        self.displayStatusLabel.text = status;
        self.statusPill.hidden = NO;
        [self.statusActivity startAnimating];
    });
}

@end

@interface VSHiftJITViewController : UIViewController <UIDocumentPickerDelegate> {
    vshift::ps3::PS3Core *_ps3Core;
}
@property(nonatomic, strong) UIScrollView *scrollView;
@property(nonatomic, strong) UIStackView *contentStack;
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UIButton *ps3BootButton;
@property(nonatomic, strong) UIButton *exportManifestButton;
@property(nonatomic, strong) NSURL *manifestURL;
@property(nonatomic, strong) NSURL *ps3FirmwareURL;
@property(nonatomic, strong) NSMutableArray<NSMutableDictionary *> *ps3Machines;
@property(nonatomic, strong) NSMutableDictionary *activePs3Machine;
@property(nonatomic, strong) VSHiftDisplayViewController *displayViewController;
@property(nonatomic, assign) VSHiftPS3Screen ps3Screen;
@property(nonatomic, assign) VSHiftPS3Screen settingsReturnScreen;
@property(nonatomic, assign) BOOL editingPs3Machines;
@property(nonatomic, assign) BOOL pickingPs3Firmware;
- (void)submitPS3FramebufferImage:(UIImage *)image;
- (void)submitPS3FramebufferBGRA8:(const std::uint8_t *)pixels
                             width:(std::size_t)width
                            height:(std::size_t)height
                       bytesPerRow:(std::size_t)bytesPerRow;
- (void)setPS3DisplayStatus:(NSString *)status;
- (void)handlePS3RuntimePowerRequest;
- (void)handlePS3RuntimePauseRequest:(BOOL)paused;
@end

@implementation VSHiftJITViewController

- (void)dealloc {
    delete _ps3Core;
    _ps3Core = nullptr;
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

static std::uint64_t ReadU64BE(const std::uint8_t *bytes) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = (value << 8) | bytes[index];
    }
    return value;
}

- (UIColor *)cardColor {
    return UIColor.secondarySystemGroupedBackgroundColor;
}

- (UIColor *)secondaryCardColor {
    return UIColor.tertiarySystemGroupedBackgroundColor;
}

- (UILabel *)labelWithText:(NSString *)text
                       font:(UIFont *)font
                      color:(UIColor *)color {
    UILabel *label = [[UILabel alloc] init];
    label.text = text;
    label.font = font;
    label.textColor = color;
    label.numberOfLines = 0;
    label.adjustsFontForContentSizeCategory = YES;
    return label;
}

- (UIButton *)buttonWithTitle:(NSString *)title
                        symbol:(NSString *)symbol
                          style:(VSHiftButtonStyle)style
                        action:(SEL)action {
    UIButtonConfiguration *configuration = nil;
    switch (style) {
    case VSHiftButtonStyleFilled:
        configuration = [UIButtonConfiguration filledButtonConfiguration];
        break;
    case VSHiftButtonStyleGray:
        configuration = [UIButtonConfiguration grayButtonConfiguration];
        break;
    case VSHiftButtonStyleTinted:
        configuration = [UIButtonConfiguration tintedButtonConfiguration];
        break;
    }
    configuration.title = title;
    configuration.image = [UIImage systemImageNamed:symbol];
    configuration.imagePadding = 8.0;
    configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    configuration.baseForegroundColor = style == VSHiftButtonStyleFilled
        ? UIColor.whiteColor
        : UIColor.labelColor;
    if (style == VSHiftButtonStyleFilled) {
        configuration.baseBackgroundColor = UIColor.systemBlueColor;
    }
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    button.configuration = configuration;
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (void)prepareScreen {
    for (UIView *subview in self.view.subviews.copy) {
        [subview removeFromSuperview];
    }
    self.view.backgroundColor = UIColor.systemGroupedBackgroundColor;

    self.scrollView = [[UIScrollView alloc] init];
    self.scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.scrollView.alwaysBounceVertical = YES;
    self.scrollView.showsVerticalScrollIndicator = NO;
    [self.view addSubview:self.scrollView];

    self.contentStack = [[UIStackView alloc] init];
    self.contentStack.translatesAutoresizingMaskIntoConstraints = NO;
    self.contentStack.axis = UILayoutConstraintAxisVertical;
    self.contentStack.spacing = 20.0;
    UIView *contentContainer = [[UIView alloc] initWithFrame:CGRectZero];
    contentContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [self.scrollView addSubview:contentContainer];
    [contentContainer addSubview:self.contentStack];

    [NSLayoutConstraint activateConstraints:@[
        [self.scrollView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [self.scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.scrollView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [contentContainer.leadingAnchor constraintEqualToAnchor:self.scrollView.contentLayoutGuide.leadingAnchor],
        [contentContainer.trailingAnchor constraintEqualToAnchor:self.scrollView.contentLayoutGuide.trailingAnchor],
        [contentContainer.topAnchor constraintEqualToAnchor:self.scrollView.contentLayoutGuide.topAnchor],
        [contentContainer.bottomAnchor constraintEqualToAnchor:self.scrollView.contentLayoutGuide.bottomAnchor],
        [contentContainer.widthAnchor constraintEqualToAnchor:self.scrollView.frameLayoutGuide.widthAnchor],
        [self.contentStack.topAnchor constraintEqualToAnchor:contentContainer.topAnchor],
        [self.contentStack.bottomAnchor constraintEqualToAnchor:contentContainer.bottomAnchor constant:-28.0],
        [self.contentStack.centerXAnchor constraintEqualToAnchor:contentContainer.centerXAnchor],
        [self.contentStack.leadingAnchor constraintGreaterThanOrEqualToAnchor:contentContainer.leadingAnchor constant:16.0],
        [self.contentStack.trailingAnchor constraintLessThanOrEqualToAnchor:contentContainer.trailingAnchor constant:-16.0],
        [self.contentStack.widthAnchor constraintLessThanOrEqualToConstant:720.0],
    ]];
    NSLayoutConstraint *fillWidth =
        [self.contentStack.widthAnchor constraintEqualToAnchor:contentContainer.widthAnchor constant:-32.0];
    fillWidth.priority = UILayoutPriorityDefaultHigh;
    fillWidth.active = YES;
}

- (void)addSectionTitle:(NSString *)title {
    UILabel *label = [self labelWithText:title
                                    font:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
                                   color:UIColor.secondaryLabelColor];
    [self.contentStack addArrangedSubview:label];
}

- (void)configureNavigationWithTitle:(NSString *)title
                          largeTitle:(BOOL)largeTitle
                                left:(NSArray<UIBarButtonItem *> *)left
                               right:(NSArray<UIBarButtonItem *> *)right {
    self.navigationItem.title = title;
    self.navigationItem.largeTitleDisplayMode = largeTitle
        ? UINavigationItemLargeTitleDisplayModeAlways
        : UINavigationItemLargeTitleDisplayModeNever;
    self.navigationItem.leftBarButtonItems = left;
    self.navigationItem.rightBarButtonItems = right;
}

- (UIBarButtonItem *)barButtonWithSystemItem:(UIBarButtonSystemItem)item action:(SEL)action {
    return [[UIBarButtonItem alloc] initWithBarButtonSystemItem:item target:self action:action];
}

- (UIBarButtonItem *)barButtonWithImage:(NSString *)symbol action:(SEL)action {
    return [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:symbol]
                                            style:UIBarButtonItemStylePlain
                                           target:self
                                           action:action];
}

- (void)addDetailRow:(NSString *)title
               value:(NSString *)value
              symbol:(NSString *)symbol
               stack:(UIStackView *)stack {
    UIImageView *icon = [[UIImageView alloc] initWithImage:[UIImage systemImageNamed:symbol]];
    icon.tintColor = UIColor.labelColor;
    icon.contentMode = UIViewContentModeScaleAspectFit;
    [icon.widthAnchor constraintEqualToConstant:28.0].active = YES;

    UILabel *name = [self labelWithText:title
                                   font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                                  color:UIColor.labelColor];
    name.font = [UIFont systemFontOfSize:name.font.pointSize weight:UIFontWeightSemibold];
    UILabel *detail = [self labelWithText:value ?: @"—"
                                     font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                                    color:UIColor.secondaryLabelColor];
    detail.textAlignment = NSTextAlignmentRight;
    detail.lineBreakMode = NSLineBreakByTruncatingTail;
    UIStackView *row = [[UIStackView alloc] initWithArrangedSubviews:@[
        icon, name, [[UIView alloc] init], detail
    ]];
    row.axis = UILayoutConstraintAxisHorizontal;
    row.alignment = UIStackViewAlignmentCenter;
    row.spacing = 12.0;
    row.layoutMargins = UIEdgeInsetsMake(10.0, 16.0, 10.0, 16.0);
    row.layoutMarginsRelativeArrangement = YES;
    [row.heightAnchor constraintGreaterThanOrEqualToConstant:52.0].active = YES;
    [name setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    [detail setContentCompressionResistancePriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
    [stack addArrangedSubview:row];
}

- (NSURL *)ps3MachinesURL {
    NSArray<NSURL *> *directories = [[NSFileManager defaultManager]
        URLsForDirectory:NSApplicationSupportDirectory
               inDomains:NSUserDomainMask];
    NSURL *directory = [directories.firstObject URLByAppendingPathComponent:@"VSHift"
                                                                   isDirectory:YES];
    [[NSFileManager defaultManager] createDirectoryAtURL:directory
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];
    return [directory URLByAppendingPathComponent:@"ps3-machines.json"];
}

- (NSMutableDictionary *)normalizedMachine:(NSDictionary *)machine {
    NSMutableDictionary *result = [machine mutableCopy];
    if (result[@"id"] == nil) result[@"id"] = [NSUUID UUID].UUIDString;
    if (result[@"name"] == nil) result[@"name"] = @"PS3 Console";
    if (result[@"platform"] == nil) result[@"platform"] = @"PS3";
    if (result[@"status"] == nil) result[@"status"] = @"Stopped";
    if (result[@"architecture"] == nil) result[@"architecture"] = @"PowerPC PPU / LV2";
    if (result[@"memory"] == nil) result[@"memory"] = @"256 MB Cell + 256 MB RSX";
    if (result[@"storage"] == nil) result[@"storage"] = @"Not configured";
    return result;
}

- (void)loadPs3Machines {
    self.ps3Machines = [NSMutableArray array];
    NSData *data = [NSData dataWithContentsOfURL:self.ps3MachinesURL];
    if (data == nil) return;
    NSArray *saved = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    if (![saved isKindOfClass:NSArray.class]) return;
    for (NSDictionary *machine in saved) {
        if ([machine isKindOfClass:NSDictionary.class]) {
            [self.ps3Machines addObject:[self normalizedMachine:machine]];
        }
    }
}

- (void)savePs3Machines {
    if (self.ps3Machines == nil) return;
    NSData *data = [NSJSONSerialization dataWithJSONObject:self.ps3Machines
                                                     options:NSJSONWritingPrettyPrinted
                                                       error:nil];
    [data writeToURL:self.ps3MachinesURL options:NSDataWritingAtomic error:nil];
}

- (NSString *)machineName:(NSDictionary *)machine {
    return [machine[@"name"] isKindOfClass:NSString.class] ? machine[@"name"] : @"PS3 Console";
}

- (NSString *)machineValue:(NSDictionary *)machine
                       key:(NSString *)key
                  fallback:(NSString *)fallback {
    NSString *value = [machine[key] isKindOfClass:NSString.class] ? machine[key] : nil;
    return value.length > 0 ? value : fallback;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    [self loadPs3Machines];
    self.ps3Screen = VSHiftPS3ScreenLibrary;
    [self rebuildPs3Screen];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskPortrait;
}

- (void)rebuildPs3Screen {
    [self prepareScreen];
    switch (self.ps3Screen) {
    case VSHiftPS3ScreenLibrary:
        [self showPs3LibraryContent];
        break;
    case VSHiftPS3ScreenStart:
        [self showPs3StartContent];
        break;
    case VSHiftPS3ScreenDetail:
        [self showPs3DetailContent];
        break;
    case VSHiftPS3ScreenSettings:
        [self showPs3SettingsContent];
        break;
    }
}

- (void)showPs3Library {
    self.ps3Screen = VSHiftPS3ScreenLibrary;
    [self rebuildPs3Screen];
}

- (void)showPs3Start {
    self.ps3Screen = VSHiftPS3ScreenStart;
    [self rebuildPs3Screen];
}

- (void)showPs3Detail {
    if (self.activePs3Machine == nil) {
        [self showPs3Library];
        return;
    }
    self.ps3Screen = VSHiftPS3ScreenDetail;
    NSString *path = self.activePs3Machine[@"firmware_path"];
    if (self.ps3FirmwareURL == nil && path.length > 0) {
        NSURL *url = [NSURL fileURLWithPath:path];
        if ([[NSFileManager defaultManager] fileExistsAtPath:url.path]) {
            self.ps3FirmwareURL = url;
        }
    }
    [self rebuildPs3Screen];
}

- (void)showPs3Settings {
    self.ps3Screen = VSHiftPS3ScreenSettings;
    [self rebuildPs3Screen];
}

- (void)showPs3LibrarySettings {
    self.settingsReturnScreen = VSHiftPS3ScreenLibrary;
    [self showPs3Settings];
}

- (void)showPs3DetailSettings {
    self.settingsReturnScreen = VSHiftPS3ScreenDetail;
    [self showPs3Settings];
}

- (void)showPs3LibraryContent {
    UIBarButtonItem *add = [self barButtonWithSystemItem:UIBarButtonSystemItemAdd action:@selector(showPs3Start)];
    UIBarButtonItem *settings = [self barButtonWithImage:@"gearshape" action:@selector(showPs3LibrarySettings)];
    UIBarButtonItem *edit = [[UIBarButtonItem alloc]
        initWithTitle:self.editingPs3Machines ? @"Done" : @"Edit"
                style:self.editingPs3Machines ? UIBarButtonItemStyleDone : UIBarButtonItemStylePlain
               target:self
               action:@selector(togglePs3Editing)];
    [self configureNavigationWithTitle:@"VSHift PS3"
                             largeTitle:YES
                                   left:@[add]
                                  right:@[edit, settings]];

    if (self.ps3Machines.count == 0) {
        UIImageView *icon = [[UIImageView alloc] initWithImage:[UIImage systemImageNamed:@"gamecontroller"]];
        icon.tintColor = UIColor.secondaryLabelColor;
        icon.contentMode = UIViewContentModeScaleAspectFit;
        [icon.heightAnchor constraintEqualToConstant:54.0].active = YES;
        UILabel *emptyTitle = [self labelWithText:@"No Virtual Consoles"
                                             font:[UIFont preferredFontForTextStyle:UIFontTextStyleTitle2]
                                            color:UIColor.labelColor];
        emptyTitle.textAlignment = NSTextAlignmentCenter;
        UILabel *emptyText = [self labelWithText:@"Create a console, import PS3UPDAT.PUP, then start it from the console screen."
                                            font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                                           color:UIColor.secondaryLabelColor];
        emptyText.textAlignment = NSTextAlignmentCenter;
        UIButton *create = [self buttonWithTitle:@"Create Virtual Console"
                                          symbol:@"plus"
                                            style:VSHiftButtonStyleFilled
                                          action:@selector(showPs3Start)];
        [create.heightAnchor constraintGreaterThanOrEqualToConstant:50.0].active = YES;
        UIStackView *emptyStack = [[UIStackView alloc] initWithArrangedSubviews:@[icon, emptyTitle, emptyText, create]];
        emptyStack.alignment = UIStackViewAlignmentFill;
        emptyStack.spacing = 12.0;
        emptyStack.layoutMargins = UIEdgeInsetsMake(56.0, 24.0, 56.0, 24.0);
        emptyStack.layoutMarginsRelativeArrangement = YES;
        [self.contentStack addArrangedSubview:emptyStack];
        return;
    }

    [self addSectionTitle:@"Virtual consoles"];
    for (NSInteger index = 0; index < self.ps3Machines.count; ++index) {
        NSMutableDictionary *machine = self.ps3Machines[index];
        NSString *subtitle = [NSString stringWithFormat:@"%@  •  %@",
            [self machineValue:machine key:@"platform" fallback:@"PS3"],
            [self machineValue:machine key:@"status" fallback:@"Stopped"]];
        UIButtonConfiguration *configuration = [UIButtonConfiguration grayButtonConfiguration];
        configuration.title = [self machineName:machine];
        configuration.subtitle = subtitle;
        configuration.image = [UIImage systemImageNamed:@"gamecontroller.fill"];
        configuration.imagePadding = 14.0;
        configuration.baseForegroundColor = UIColor.labelColor;
        configuration.baseBackgroundColor = [self secondaryCardColor];
        configuration.cornerStyle = UIButtonConfigurationCornerStyleLarge;
        configuration.titleAlignment = UIButtonConfigurationTitleAlignmentLeading;
        configuration.contentInsets = NSDirectionalEdgeInsetsMake(14.0, 16.0, 14.0, 16.0);
        UIButton *machineButton = [UIButton buttonWithType:UIButtonTypeSystem];
        machineButton.configuration = configuration;
        machineButton.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeading;
        [machineButton.heightAnchor constraintGreaterThanOrEqualToConstant:76.0].active = YES;
        machineButton.tag = index;
        [machineButton addTarget:self action:@selector(openPs3Machine:) forControlEvents:UIControlEventTouchUpInside];

        if (!self.editingPs3Machines) {
            [self.contentStack addArrangedSubview:machineButton];
        } else {
            UIButton *deleteButton = [self buttonWithTitle:@""
                                                    symbol:@"trash"
                                                      style:VSHiftButtonStyleGray
                                                    action:@selector(deletePs3MachineFromLibrary:)];
            deleteButton.tintColor = UIColor.systemRedColor;
            deleteButton.tag = index;
            UIStackView *row = [[UIStackView alloc] initWithArrangedSubviews:@[machineButton, deleteButton]];
            row.axis = UILayoutConstraintAxisHorizontal;
            row.spacing = 8.0;
            [self.contentStack addArrangedSubview:row];
        }
    }
}

- (void)showPs3StartContent {
    UIBarButtonItem *cancel = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                                                                            target:self
                                                                            action:@selector(showPs3Library)];
    [self configureNavigationWithTitle:@"Start" largeTitle:YES left:@[cancel] right:@[]];

    [self addSectionTitle:@"Custom"];
    UIButton *newButton = [UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration *newConfiguration = [UIButtonConfiguration grayButtonConfiguration];
    newConfiguration.title = @"New virtual console";
    newConfiguration.subtitle = @"Create a PS3 console profile from scratch.";
    newConfiguration.image = [UIImage systemImageNamed:@"gamecontroller"];
    newConfiguration.imagePadding = 16.0;
    newConfiguration.baseForegroundColor = UIColor.labelColor;
    newConfiguration.baseBackgroundColor = [self secondaryCardColor];
    newConfiguration.cornerStyle = UIButtonConfigurationCornerStyleLarge;
    newConfiguration.titleAlignment = UIButtonConfigurationTitleAlignmentCenter;
    newConfiguration.imagePlacement = NSDirectionalRectEdgeTop;
    newConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(26.0, 18.0, 26.0, 18.0);
    newButton.configuration = newConfiguration;
    [newButton.heightAnchor constraintGreaterThanOrEqualToConstant:154.0].active = YES;
    [newButton addTarget:self action:@selector(createPs3Machine) forControlEvents:UIControlEventTouchUpInside];
    [self.contentStack addArrangedSubview:newButton];

    [self addSectionTitle:@"Existing"];
    UIButton *importButton = [self buttonWithTitle:@"Import PS3UPDAT.PUP"
                                            symbol:@"doc.badge.plus"
                                              style:VSHiftButtonStyleTinted
                                            action:@selector(createPs3MachineAndImport)];
    [self.contentStack addArrangedSubview:importButton];
    [importButton.heightAnchor constraintGreaterThanOrEqualToConstant:52.0].active = YES;
}

- (UIView *)displayCard {
    UIView *display = [[UIView alloc] init];
    display.backgroundColor = UIColor.blackColor;
    display.layer.cornerRadius = 18.0;
    display.layer.cornerCurve = kCACornerCurveContinuous;
    display.clipsToBounds = YES;
    UIButtonConfiguration *playConfiguration = [UIButtonConfiguration plainButtonConfiguration];
    playConfiguration.image = [UIImage systemImageNamed:@"play.circle.fill"];
    playConfiguration.preferredSymbolConfigurationForImage =
        [UIImageSymbolConfiguration configurationWithPointSize:76.0 weight:UIImageSymbolWeightRegular];
    playConfiguration.baseForegroundColor = UIColor.whiteColor;
    UIButton *playButton = [UIButton buttonWithType:UIButtonTypeSystem];
    playButton.configuration = playConfiguration;
    [playButton addTarget:self action:@selector(startPs3Machine) forControlEvents:UIControlEventTouchUpInside];
    playButton.translatesAutoresizingMaskIntoConstraints = NO;
    [display addSubview:playButton];
    [NSLayoutConstraint activateConstraints:@[
        [display.heightAnchor constraintEqualToAnchor:display.widthAnchor multiplier:9.0 / 16.0],
        [display.heightAnchor constraintGreaterThanOrEqualToConstant:190.0],
        [playButton.centerXAnchor constraintEqualToAnchor:display.centerXAnchor],
        [playButton.centerYAnchor constraintEqualToAnchor:display.centerYAnchor],
    ]];
    return display;
}

- (void)showPs3DetailContent {
    UIBarButtonItem *back = [self barButtonWithImage:@"chevron.backward" action:@selector(showPs3Library)];
    back.accessibilityLabel = @"Library";
    UIBarButtonItem *settings = [self barButtonWithImage:@"slider.horizontal.3" action:@selector(showPs3DetailSettings)];
    [self configureNavigationWithTitle:[self machineName:self.activePs3Machine]
                             largeTitle:YES
                                   left:@[back]
                                  right:@[settings]];
    [self.contentStack addArrangedSubview:[self displayCard]];

    UIView *detailsCard = [[UIView alloc] init];
    detailsCard.backgroundColor = [self cardColor];
    detailsCard.layer.cornerRadius = 18.0;
    detailsCard.layer.cornerCurve = kCACornerCurveContinuous;
    detailsCard.clipsToBounds = YES;
    UIStackView *details = [[UIStackView alloc] init];
    details.axis = UILayoutConstraintAxisVertical;
    details.spacing = 2.0;
    details.translatesAutoresizingMaskIntoConstraints = NO;
    [detailsCard addSubview:details];
    [NSLayoutConstraint activateConstraints:@[
        [details.leadingAnchor constraintEqualToAnchor:detailsCard.leadingAnchor],
        [details.trailingAnchor constraintEqualToAnchor:detailsCard.trailingAnchor],
        [details.topAnchor constraintEqualToAnchor:detailsCard.topAnchor],
        [details.bottomAnchor constraintEqualToAnchor:detailsCard.bottomAnchor],
    ]];
    [self addDetailRow:@"Status" value:[self machineValue:self.activePs3Machine key:@"status" fallback:@"Stopped"] symbol:@"info.circle" stack:details];
    [self addDetailRow:@"Architecture" value:[self machineValue:self.activePs3Machine key:@"architecture" fallback:@"PowerPC PPU / LV2"] symbol:@"cpu" stack:details];
    [self addDetailRow:@"Memory" value:[self machineValue:self.activePs3Machine key:@"memory" fallback:@"256 MB Cell + 256 MB RSX"] symbol:@"memorychip" stack:details];
    [self addDetailRow:@"Firmware" value:[self machineValue:self.activePs3Machine key:@"firmware_file" fallback:@"Not imported"] symbol:@"doc" stack:details];
    [self addDetailRow:@"Version" value:[self machineValue:self.activePs3Machine key:@"firmware_version" fallback:@"—"] symbol:@"number" stack:details];
    [self addDetailRow:@"Storage" value:[self machineValue:self.activePs3Machine key:@"storage" fallback:@"Not configured"] symbol:@"internaldrive" stack:details];
    [self.contentStack addArrangedSubview:detailsCard];

    UIView *statusCard = [[UIView alloc] init];
    statusCard.backgroundColor = [self cardColor];
    statusCard.layer.cornerRadius = 18.0;
    statusCard.layer.cornerCurve = kCACornerCurveContinuous;
    statusCard.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);
    UILabel *statusTitle = [self labelWithText:@"Runtime Status" font:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline] color:UIColor.labelColor];
    NSString *lastResult = [self machineValue:self.activePs3Machine key:@"last_result" fallback:@"Import a PS3 firmware file to prepare this console."];
    self.statusLabel = [self labelWithText:lastResult font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody] color:UIColor.secondaryLabelColor];
    UIStackView *statusStack = [[UIStackView alloc] initWithArrangedSubviews:@[statusTitle, self.statusLabel]];
    statusStack.axis = UILayoutConstraintAxisVertical;
    statusStack.spacing = 8.0;
    statusStack.translatesAutoresizingMaskIntoConstraints = NO;
    [statusCard addSubview:statusStack];
    [NSLayoutConstraint activateConstraints:@[
        [statusStack.leadingAnchor constraintEqualToAnchor:statusCard.layoutMarginsGuide.leadingAnchor],
        [statusStack.trailingAnchor constraintEqualToAnchor:statusCard.layoutMarginsGuide.trailingAnchor],
        [statusStack.topAnchor constraintEqualToAnchor:statusCard.layoutMarginsGuide.topAnchor],
        [statusStack.bottomAnchor constraintEqualToAnchor:statusCard.layoutMarginsGuide.bottomAnchor],
    ]];
    [self.contentStack addArrangedSubview:statusCard];

    UIButton *import = [self buttonWithTitle:@"Import PS3UPDAT.PUP" symbol:@"doc.badge.plus" style:VSHiftButtonStyleTinted action:@selector(importPs3Firmware)];
    [import.heightAnchor constraintGreaterThanOrEqualToConstant:50.0].active = YES;
    [self.contentStack addArrangedSubview:import];
    self.ps3BootButton = [self buttonWithTitle:@"Start" symbol:@"play.fill" style:VSHiftButtonStyleFilled action:@selector(startPs3Machine)];
    self.ps3BootButton.enabled = self.ps3FirmwareURL != nil;
    [self.ps3BootButton.heightAnchor constraintGreaterThanOrEqualToConstant:52.0].active = YES;
    [self.contentStack addArrangedSubview:self.ps3BootButton];
    self.exportManifestButton = [self buttonWithTitle:@"Export manifest" symbol:@"square.and.arrow.up" style:VSHiftButtonStyleGray action:@selector(exportManifest)];
    self.exportManifestButton.enabled = self.manifestURL != nil;
    [self.contentStack addArrangedSubview:self.exportManifestButton];
    [self.contentStack addArrangedSubview:[self buttonWithTitle:@"Delete console" symbol:@"trash" style:VSHiftButtonStyleGray action:@selector(deleteActivePs3Machine)]];
}

- (void)showPs3SettingsContent {
    SEL backAction = self.settingsReturnScreen == VSHiftPS3ScreenDetail
        ? @selector(showPs3Detail)
        : @selector(showPs3Library);
    UIBarButtonItem *back = [self barButtonWithImage:@"chevron.backward" action:backAction];
    [self configureNavigationWithTitle:@"Settings" largeTitle:YES left:@[back] right:@[]];
    [self addSectionTitle:@"Console"];
    [self addSettingsGroup:@[
        @[@"Information", @"info.circle", @"PS3 console profiles and firmware status"],
        @[@"System", @"cpu", @"PPU, LV2 and RSX runtime"],
        @[@"Input", @"gamecontroller", @"Controller mapping"],
        @[@"Sharing", @"person.2", @"Shared files and import locations"]
    ]];
    [self addSectionTitle:@"Devices"];
    [self addSettingsGroup:@[
        @[@"Display", @"display", @"Virtual framebuffer output"],
        @[@"Network", @"network", @"Emulated network device"],
        @[@"Storage", @"internaldrive", @"dev_flash and virtual storage"]
    ]];
}

- (void)addSettingsGroup:(NSArray<NSArray<NSString *> *> *)items {
    UIStackView *group = [[UIStackView alloc] init];
    group.axis = UILayoutConstraintAxisVertical;
    group.spacing = 1.0;
    for (NSArray<NSString *> *item in items) {
        UIButtonConfiguration *configuration = [UIButtonConfiguration grayButtonConfiguration];
        configuration.title = item[0];
        configuration.subtitle = item[2];
        configuration.image = [UIImage systemImageNamed:item[1]];
        configuration.imagePadding = 14.0;
        configuration.baseForegroundColor = UIColor.labelColor;
        configuration.baseBackgroundColor = [self cardColor];
        configuration.cornerStyle = UIButtonConfigurationCornerStyleLarge;
        configuration.titleAlignment = UIButtonConfigurationTitleAlignmentLeading;
        configuration.contentInsets = NSDirectionalEdgeInsetsMake(12.0, 16.0, 12.0, 16.0);
        UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
        button.configuration = configuration;
        button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeading;
        [button.heightAnchor constraintGreaterThanOrEqualToConstant:64.0].active = YES;
        button.accessibilityValue = item[2];
        [button addTarget:self action:@selector(showPs3SettingsInfo:) forControlEvents:UIControlEventTouchUpInside];
        [group addArrangedSubview:button];
    }
    [self.contentStack addArrangedSubview:group];
}

- (void)showPs3SettingsInfo:(UIButton *)sender {
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:sender.configuration.title
                                                                     message:sender.accessibilityValue
                                                              preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Done" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (void)togglePs3Editing {
    self.editingPs3Machines = !self.editingPs3Machines;
    [self rebuildPs3Screen];
}

- (void)openPs3Machine:(UIButton *)sender {
    if (sender.tag < 0 || sender.tag >= self.ps3Machines.count) return;
    self.activePs3Machine = self.ps3Machines[sender.tag];
    self.ps3FirmwareURL = nil;
    self.manifestURL = nil;
    [self showPs3Detail];
}

- (void)createPs3Machine {
    [self promptCreatePs3MachineWithImport:NO];
}

- (void)createPs3MachineAndImport {
    [self promptCreatePs3MachineWithImport:YES];
}

- (void)promptCreatePs3MachineWithImport:(BOOL)importFirmware {
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"New PS3 console"
                                                                     message:@"Choose a name for the virtual console."
                                                              preferredStyle:UIAlertControllerStyleAlert];
    [alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
        field.placeholder = @"My PS3";
        field.text = @"PS3 Console";
    }];
    __block VSHiftJITViewController *blockSelf = self;
    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Create" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        (void)action;
        VSHiftJITViewController *strongSelf = blockSelf;
        if (strongSelf == nil) return;
        NSString *name = alert.textFields.firstObject.text;
        if (name.length == 0) name = @"PS3 Console";
        NSMutableDictionary *machine = [strongSelf normalizedMachine:@{
            @"id": [NSUUID UUID].UUIDString,
            @"name": name,
            @"platform": @"PS3",
            @"status": @"Stopped",
        }];
        [strongSelf.ps3Machines addObject:machine];
        strongSelf.activePs3Machine = machine;
        [strongSelf savePs3Machines];
        [strongSelf showPs3Detail];
        if (importFirmware) [strongSelf importPs3Firmware];
    }]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (void)deletePs3MachineFromLibrary:(UIButton *)sender {
    if (sender.tag < 0 || sender.tag >= self.ps3Machines.count) return;
    [self.ps3Machines removeObjectAtIndex:sender.tag];
    [self savePs3Machines];
    [self rebuildPs3Screen];
}

- (void)deleteActivePs3Machine {
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Delete console?"
                                                                     message:@"The profile will be removed. Imported files are not deleted."
                                                              preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    __block VSHiftJITViewController *blockSelf = self;
    [alert addAction:[UIAlertAction actionWithTitle:@"Delete" style:UIAlertActionStyleDestructive handler:^(UIAlertAction *action) {
        (void)action;
        VSHiftJITViewController *strongSelf = blockSelf;
        if (strongSelf == nil) return;
        [strongSelf.ps3Machines removeObject:strongSelf.activePs3Machine];
        strongSelf.activePs3Machine = nil;
        strongSelf.ps3FirmwareURL = nil;
        [strongSelf savePs3Machines];
        [strongSelf showPs3Library];
    }]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (void)startPs3Machine {
    if (self.ps3FirmwareURL == nil) {
        UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Firmware Required"
                                                                         message:@"Import PS3UPDAT.PUP before starting this virtual console."
                                                                  preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
        [alert addAction:[UIAlertAction actionWithTitle:@"Import"
                                                  style:UIAlertActionStyleDefault
                                                handler:^(UIAlertAction *action) {
            (void)action;
            [self importPs3Firmware];
        }]];
        [self presentViewController:alert animated:YES completion:nil];
        return;
    }
    self.activePs3Machine[@"status"] = @"Starting";
    self.activePs3Machine[@"last_result"] = @"Starting PS3 runtime…";
    [self savePs3Machines];
    delete _ps3Core;
    _ps3Core = nullptr;

    VSHiftDisplayViewController *display = [[VSHiftDisplayViewController alloc] init];
    display.modalPresentationStyle = UIModalPresentationFullScreen;
    display.modalTransitionStyle = UIModalTransitionStyleCrossDissolve;
    display.modalPresentationCapturesStatusBarAppearance = YES;
    self.displayViewController = display;
    __block VSHiftJITViewController *blockSelf = self;
    display.closeHandler = ^{
        VSHiftJITViewController *strongSelf = blockSelf;
        strongSelf.displayViewController = nil;
        [strongSelf rebuildPs3Screen];
    };
    display.powerHandler = ^{
        VSHiftJITViewController *strongSelf = blockSelf;
        [strongSelf handlePS3RuntimePowerRequest];
    };
    display.pauseHandler = ^(BOOL paused) {
        VSHiftJITViewController *strongSelf = blockSelf;
        [strongSelf handlePS3RuntimePauseRequest:paused];
    };
    [self presentViewController:display animated:YES completion:^{
        [blockSelf preparePs3VshBootstrap];
    }];
}

// Runtime UI contract: call these methods on the controller that owns the console.
// Frame delivery is marshalled onto the main queue and remains valid while the
// fullscreen display controller is presented.
- (void)submitPS3FramebufferImage:(UIImage *)image {
    [self.displayViewController updateFramebufferImage:image];
}

- (void)submitPS3FramebufferBGRA8:(const std::uint8_t *)pixels
                             width:(std::size_t)width
                            height:(std::size_t)height
                       bytesPerRow:(std::size_t)bytesPerRow {
    if (pixels == nullptr || width == 0 || height == 0 ||
        width > std::numeric_limits<std::size_t>::max() / 4 || bytesPerRow < width * 4 ||
        height > std::numeric_limits<std::size_t>::max() / bytesPerRow) {
        return;
    }
    const std::size_t byteCount = height * bytesPerRow;
    if (byteCount > static_cast<std::size_t>(std::numeric_limits<CFIndex>::max())) return;
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, pixels, static_cast<CFIndex>(byteCount));
    if (data == nullptr) return;
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(width,
                                     height,
                                     8,
                                     32,
                                     bytesPerRow,
                                     colorSpace,
                                     static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little |
                                                               kCGImageAlphaPremultipliedFirst),
                                     provider,
                                     nullptr,
                                     false,
                                     kCGRenderingIntentDefault);
    if (image != nullptr) {
        [self submitPS3FramebufferImage:[UIImage imageWithCGImage:image]];
        CGImageRelease(image);
    }
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
    CFRelease(data);
}

- (void)setPS3DisplayStatus:(NSString *)status {
    self.statusLabel.text = status;
    [self.displayViewController updateDisplayStatus:status];
}

- (void)handlePS3RuntimePowerRequest {
    if (_ps3Core != nullptr) _ps3Core->shutdown();
    self.activePs3Machine[@"status"] = @"Stopped";
    self.activePs3Machine[@"last_result"] = @"Virtual console powered off.";
    [self savePs3Machines];
}

- (void)handlePS3RuntimePauseRequest:(BOOL)paused {
    if (_ps3Core != nullptr) {
        if (paused) {
            _ps3Core->pause();
        } else {
            _ps3Core->resume();
        }
    }
    self.activePs3Machine[@"status"] = paused ? @"Paused" : @"Running";
    [self savePs3Machines];
}

- (void)importPs3Firmware {
    self.pickingPs3Firmware = YES;
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeData]
                                 asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)exportManifest {
    if (self.manifestURL == nil) {
        self.statusLabel.text = @"Import a PS3 firmware file first to create a manifest.";
        return;
    }
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc]
            initForExportingURLs:@[self.manifestURL]
                           asCopy:YES];
    picker.delegate = self;
    [self presentViewController:picker animated:YES completion:nil];
}

- (BOOL)writeManifest:(NSDictionary *)manifest error:(NSError **)error {
    NSArray<NSURL *> *directories = [[NSFileManager defaultManager]
        URLsForDirectory:NSApplicationSupportDirectory
               inDomains:NSUserDomainMask];
    NSURL *directory = [directories.firstObject URLByAppendingPathComponent:@"VSHift"
                                                                    isDirectory:YES];
    [[NSFileManager defaultManager] createDirectoryAtURL:directory
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:error];
    NSData *data = [NSJSONSerialization dataWithJSONObject:manifest
                                                     options:NSJSONWritingPrettyPrinted
                                                       error:error];
    if (data == nil) return NO;
    NSURL *url = [directory URLByAppendingPathComponent:@"firmware-manifest.json"];
    BOOL saved = [data writeToURL:url options:NSDataWritingAtomic error:error];
    if (saved) {
        self.manifestURL = url;
        if (self.exportManifestButton != nil) self.exportManifestButton.enabled = YES;
    }
    return saved;
}

- (void)preparePs3VshBootstrap {
#if defined(VSHIFT_HAVE_RPCS3_CORE)
    // Force-link the platform-independent RPCS3 archive without its Qt
    // desktop frontend. The native RSX callback bridge is added separately.
    vshift_rpcs3_ios_link_core();
#endif
    NSURL *url = self.ps3FirmwareURL;
    if (url == nil) {
        [self setPS3DisplayStatus:@"Import PS3UPDAT.PUP first."];
        return;
    }
    const BOOL accessed = [url startAccessingSecurityScopedResource];
    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:url.path error:nil];
    NSNumber *fileSizeNumber = attributes[NSFileSize];
    std::uint64_t fileSize = fileSizeNumber != nil ? fileSizeNumber.unsignedLongLongValue : 0;
    if (fileSize < vshift::firmware::kPs3PupHeaderSize ||
        fileSize > static_cast<std::uint64_t>(std::numeric_limits<NSUInteger>::max())) {
        [self setPS3DisplayStatus:@"PS3 PUP is too small or too large to read."];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    NSData *pupData = [NSData dataWithContentsOfURL:url options:NSDataReadingMappedIfSafe error:nil];
    if (pupData.length != static_cast<NSUInteger>(fileSize)) {
        [self setPS3DisplayStatus:@"Could not read PS3UPDAT.PUP."];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto *pupBytes = static_cast<const std::uint8_t *>(pupData.bytes);
    const auto headerLength = ReadU64BE(pupBytes + 0x20);
    if (headerLength < vshift::firmware::kPs3PupHeaderSize ||
        headerLength > 16ull * 1024ull * 1024ull || headerLength > fileSize) {
        [self setPS3DisplayStatus:@"PS3 PUP header size is invalid."];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto parsed = vshift::firmware::ParsePs3PupHeaders(
        std::span<const std::uint8_t>(pupBytes, static_cast<std::size_t>(headerLength)), fileSize);
    if (!parsed.ok()) {
        [self setPS3DisplayStatus:[NSString stringWithFormat:@"PS3 PUP parse failed:\n%s", parsed.error.c_str()]];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto updateEntry = std::find_if(parsed.entries.begin(), parsed.entries.end(), [](const auto &entry) {
        return entry.entry_id == 0x300;
    });
    if (updateEntry == parsed.entries.end() ||
        updateEntry->data_offset > pupData.length ||
        updateEntry->data_length > pupData.length - static_cast<NSUInteger>(updateEntry->data_offset)) {
        [self setPS3DisplayStatus:@"PS3 PUP has no readable update TAR."];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto *updateTarBytes = pupBytes + static_cast<std::size_t>(updateEntry->data_offset);
    const auto updateTar = vshift::firmware::ParsePs3Tar(
        std::span<const std::uint8_t>(updateTarBytes, static_cast<std::size_t>(updateEntry->data_length)));
    if (!updateTar.ok()) {
        [self setPS3DisplayStatus:[NSString stringWithFormat:@"PS3 update TAR parse failed:\n%s", updateTar.error.c_str()]];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto packageEntry = std::find_if(updateTar.entries.begin(), updateTar.entries.end(), [](const auto &entry) {
        return entry.regular_file && entry.name.rfind("dev_flash_012", 0) == 0;
    });
    if (packageEntry == updateTar.entries.end()) {
        [self setPS3DisplayStatus:@"dev_flash_012 package is missing."];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto packageBegin = static_cast<std::size_t>(packageEntry->data_offset);
    const auto packageLength = static_cast<std::size_t>(packageEntry->data_length);
    const auto package = vshift::firmware::DecryptPs3ScePackage(
        std::span<const std::uint8_t>(updateTarBytes + packageBegin, packageLength));
    if (!package.ok()) {
        [self setPS3DisplayStatus:[NSString stringWithFormat:@"dev_flash_012 decrypt failed:\n%s", package.error.c_str()]];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    std::uint64_t vshSize = 0;
    std::vector<std::uint8_t> vshBytes;
    for (const auto &section : package.sections) {
        const auto candidate = vshift::firmware::ParsePs3Tar(section.bytes);
        if (!candidate.ok()) continue;
        const auto found = std::find_if(candidate.entries.begin(), candidate.entries.end(), [](const auto &entry) {
            return entry.regular_file && entry.name == "dev_flash/vsh/module/vsh.self";
        });
        if (found != candidate.entries.end()) {
            vshSize = found->data_length;
            if (found->data_offset <= section.bytes.size() &&
                found->data_length <= section.bytes.size() -
                    static_cast<std::size_t>(found->data_offset)) {
                vshBytes.assign(
                    section.bytes.begin() + static_cast<std::size_t>(found->data_offset),
                    section.bytes.begin() + static_cast<std::size_t>(
                        found->data_offset + found->data_length));
            }
            break;
        }
    }
    if (accessed) [url stopAccessingSecurityScopedResource];
    if (vshSize == 0 || vshBytes.empty()) {
        [self setPS3DisplayStatus:@"dev_flash_012 decrypted, but vsh.self was not found."];
        return;
    }

    const auto selfImage = vshift::loader::ParsePs3Self(vshBytes);
    if (!selfImage.ok()) {
        [self setPS3DisplayStatus:[NSString stringWithFormat:
            @"vsh.self parse failed:\n%s", selfImage.error.c_str()]];
        self.activePs3Machine[@"status"] = @"VSH SELF rejected";
        self.activePs3Machine[@"last_result"] = [NSString stringWithFormat:
            @"The real vsh.self was found, but its PS3 SELF loader rejected it: %@",
            [NSString stringWithUTF8String:selfImage.error.c_str()]];
        [self savePs3Machines];
        return;
    }
    vshift::memory::GuestMemory guestMemory;
    const auto loaded = vshift::loader::LoadPs3SelfIntoMemory(
        selfImage.image, guestMemory);
    if (!loaded.ok()) {
        [self setPS3DisplayStatus:[NSString stringWithFormat:
            @"vsh.self load failed:\n%s", loaded.error.c_str()]];
        self.activePs3Machine[@"status"] = @"VSH image rejected";
        self.activePs3Machine[@"last_result"] = [NSString stringWithFormat:
            @"SELF metadata was decrypted, but the guest loader could not map VSH: %@",
            [NSString stringWithUTF8String:loaded.error.c_str()]];
        [self savePs3Machines];
        return;
    }

    _ps3Core = new vshift::ps3::PS3Core();
    const auto initialized = _ps3Core->initialize();
    const auto installed = initialized.success
        ? _ps3Core->installFirmware(std::span<const std::uint8_t>(pupBytes, pupData.length))
        : initialized;
    if (!installed.success) {
        [self setPS3DisplayStatus:[NSString stringWithFormat:
            @"PS3 core firmware install failed:\n%s", installed.message.c_str()]];
        self.activePs3Machine[@"status"] = @"Runtime load failed";
        self.activePs3Machine[@"last_result"] = [NSString stringWithUTF8String:
            installed.message.c_str()];
        [self savePs3Machines];
        return;
    }
    const auto booted = _ps3Core->boot();
    const auto coreStatus = _ps3Core->status();
    if (!booted.success) {
        [self setPS3DisplayStatus:[NSString stringWithFormat:
            @"PS3 boot stopped:\n%s", booted.message.c_str()]];
        self.activePs3Machine[@"status"] = @"PS3 boot stopped";
        self.activePs3Machine[@"last_result"] = [NSString stringWithUTF8String:
            booted.message.c_str()];
        [self savePs3Machines];
        return;
    }
    NSString *ppuReason = [NSString stringWithUTF8String:coreStatus.stop_reason.c_str()];
    if (ppuReason.length == 0) ppuReason = @"unknown";
    const std::size_t ppuInstructions = coreStatus.instructions;
    const std::uint64_t ppuPC = coreStatus.pc;
    const std::uint64_t ppuSyscall = coreStatus.register11;

    NSString *version = [self machineValue:self.activePs3Machine key:@"firmware_version" fallback:@"unknown"];
    NSError *manifestError = nil;
    NSDictionary *manifest = @{
        @"schema_version": @4,
        @"source_file_name": url.lastPathComponent ?: @"PS3UPDAT.PUP",
        @"source_kind": @"user-provided PS3 PUP",
        @"boot_profile": @"PS3 VSH bootstrap",
        @"firmware_version": version,
        @"package": [NSString stringWithUTF8String:packageEntry->name.c_str()],
        @"decrypted_section_count": @(package.sections.size()),
        @"vsh_self": @{
            @"path": @"dev_flash/vsh/module/vsh.self",
            @"size": @(vshSize),
            @"elf_class": @(selfImage.image.elf_class),
            @"elf_machine": @(selfImage.image.elf_machine),
            @"entry_point": @(selfImage.image.entry_point),
            @"mapped_segments": @(loaded.loaded_segments),
        },
        @"ppu_attempt": @{
            @"stop_reason": ppuReason,
            @"instructions": @(ppuInstructions),
            @"pc": @(ppuPC),
            @"syscall": @(ppuSyscall),
        },
        @"boot_status": @"PS3 PPU/LV2 execution slice completed; RSX framebuffer pending",
    };
    const BOOL manifestSaved = [self writeManifest:manifest error:&manifestError];
    self.activePs3Machine[@"status"] = [NSString stringWithFormat:@"PPU %@", ppuReason];
    self.activePs3Machine[@"vsh_self_size"] = @(vshSize);
    self.activePs3Machine[@"vsh_entry_point"] = @(selfImage.image.entry_point);
    self.activePs3Machine[@"vsh_mapped_segments"] = @(loaded.loaded_segments);
    self.activePs3Machine[@"last_result"] = [NSString stringWithFormat:
        @"VSH image loaded\nFirmware: %@\nPackage: %@\nDecrypted sections: %lu\nvsh.self: %llu bytes\nELF: %@ PowerPC\nEntry: 0x%llx\nMapped PT_LOAD: %lu\nPPU: %@ after %lu instructions\nPC: 0x%llx · syscall/r11: 0x%llx\n%@",
        version,
        [NSString stringWithUTF8String:packageEntry->name.c_str()],
        static_cast<unsigned long>(package.sections.size()),
        static_cast<unsigned long long>(vshSize),
        selfImage.image.elf_class == 2 ? @"64-bit big-endian" : @"32-bit big-endian",
        static_cast<unsigned long long>(selfImage.image.entry_point),
        static_cast<unsigned long>(loaded.loaded_segments),
        ppuReason,
        static_cast<unsigned long>(ppuInstructions),
        static_cast<unsigned long long>(ppuPC),
        static_cast<unsigned long long>(ppuSyscall),
        manifestSaved ? @"Next: RSX command processing and framebuffer."
                      : [NSString stringWithFormat:@"Manifest save failed: %@", manifestError.localizedDescription ?: @"unknown error"]];
    [self savePs3Machines];
    [self setPS3DisplayStatus:[NSString stringWithFormat:
        @"VSH runtime loaded\nPPU %@ · syscall/r11 0x%llx\nRSX framebuffer is next…",
        ppuReason, static_cast<unsigned long long>(ppuSyscall)]];
    if (self.displayViewController == nil) [self showPs3Detail];
}

- (void)inspectPs3FirmwareAtURL:(NSURL *)url {
    const BOOL accessed = [url startAccessingSecurityScopedResource];
    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:url.path error:nil];
    NSNumber *fileSizeNumber = attributes[NSFileSize];
    const std::uint64_t fileSize = fileSizeNumber != nil ? fileSizeNumber.unsignedLongLongValue : 0;
    if (fileSize < vshift::firmware::kPs3PupHeaderSize ||
        fileSize > static_cast<std::uint64_t>(std::numeric_limits<NSUInteger>::max())) {
        self.statusLabel.text = @"PS3 PUP is too small or unreadable.";
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    NSFileHandle *handle = [NSFileHandle fileHandleForReadingFromURL:url error:nil];
    if (handle == nil) {
        self.statusLabel.text = @"Could not open PS3UPDAT.PUP.";
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    NSData *fixedHeaderData = [handle readDataOfLength:vshift::firmware::kPs3PupHeaderSize];
    if (fixedHeaderData.length < vshift::firmware::kPs3PupHeaderSize) {
        [handle closeFile];
        self.statusLabel.text = @"Could not read the PS3 PUP header.";
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto *fixedHeaderBytes = static_cast<const std::uint8_t *>(fixedHeaderData.bytes);
    const auto headerLength = ReadU64BE(fixedHeaderBytes + 0x20);
    if (headerLength < vshift::firmware::kPs3PupHeaderSize ||
        headerLength > 16ull * 1024ull * 1024ull || headerLength > fileSize) {
        [handle closeFile];
        self.statusLabel.text = @"PS3 PUP header size is invalid.";
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    [handle seekToFileOffset:0];
    NSData *headerData = [handle readDataOfLength:static_cast<NSUInteger>(headerLength)];
    const auto *headerBytes = static_cast<const std::uint8_t *>(headerData.bytes);
    const auto parsed = vshift::firmware::ParsePs3PupHeaders(
        std::span<const std::uint8_t>(headerBytes, headerData.length), fileSize);
    if (!parsed.ok()) {
        [handle closeFile];
        self.statusLabel.text = [NSString stringWithFormat:@"PS3 PUP parse failed:\n%s", parsed.error.c_str()];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }

    NSMutableArray *entries = [NSMutableArray arrayWithCapacity:parsed.entries.size()];
    NSMutableArray *sceEntries = [NSMutableArray array];
    NSString *firmwareVersion = nil;
    for (const auto &entry : parsed.entries) {
        NSMutableDictionary *entryManifest = [@{
            @"id": @(entry.entry_id),
            @"offset": @(entry.data_offset),
            @"size": @(entry.data_length),
        } mutableCopy];
        if (entry.entry_id == 0x100 && entry.data_length != 0) {
            [handle seekToFileOffset:entry.data_offset];
            NSData *versionData = [handle readDataOfLength:static_cast<NSUInteger>(std::min<std::uint64_t>(entry.data_length, 64))];
            firmwareVersion = [[NSString alloc] initWithData:versionData encoding:NSUTF8StringEncoding];
            firmwareVersion = [firmwareVersion stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        }
        if (entry.data_length >= vshift::loader::kPs3SceHeaderSize) {
            [handle seekToFileOffset:entry.data_offset];
            NSData *sceData = [handle readDataOfLength:vshift::loader::kPs3SceHeaderSize];
            if (sceData.length == vshift::loader::kPs3SceHeaderSize) {
                const auto *sceBytes = static_cast<const std::uint8_t *>(sceData.bytes);
                const auto sce = vshift::loader::ParsePs3SceHeader(
                    std::span<const std::uint8_t>(sceBytes, sceData.length), entry.data_length);
                if (sce.ok()) {
                    entryManifest[@"sce"] = @{
                        @"header_version": @(sce.header.header_version),
                        @"flags": @(sce.header.flags),
                        @"type": @(sce.header.type),
                        @"metadata_offset": @(sce.header.metadata_offset),
                        @"header_size": @(sce.header.header_size),
                        @"payload_size": @(sce.header.payload_size),
                        @"metadata_encrypted": @(sce.header.metadata_encrypted()),
                    };
                    [sceEntries addObject:entryManifest];
                }
            }
        }
        [entries addObject:entryManifest];
    }
    [handle closeFile];
    if (accessed) [url stopAccessingSecurityScopedResource];

    self.ps3FirmwareURL = url;
    self.activePs3Machine[@"firmware_path"] = url.path ?: @"";
    self.activePs3Machine[@"firmware_file"] = url.lastPathComponent ?: @"PS3UPDAT.PUP";
    self.activePs3Machine[@"firmware_version"] = firmwareVersion ?: @"unknown";
    self.activePs3Machine[@"storage"] = [NSString stringWithFormat:@"PUP · %llu bytes", static_cast<unsigned long long>(fileSize)];
    self.activePs3Machine[@"status"] = @"Firmware imported";

    NSDictionary *manifest = @{
        @"schema_version": @3,
        @"source_file_name": url.lastPathComponent ?: @"PS3UPDAT.PUP",
        @"source_kind": @"user-provided PS3 PUP",
        @"container_format": @"PS3 SCEUF/PUP",
        @"container_size": @(fileSize),
        @"pup": @{
            @"package_version": @(parsed.header.package_version),
            @"image_version": @(parsed.header.image_version),
            @"file_count": @(parsed.header.file_count),
            @"header_length": @(parsed.header.header_length),
            @"data_length": @(parsed.header.data_length),
        },
        @"entries": entries,
    };
    NSError *manifestError = nil;
    const BOOL manifestSaved = [self writeManifest:manifest error:&manifestError];
    NSMutableString *summary = [NSMutableString stringWithFormat:
        @"PS3 PUP OK\n%@\nSize: %llu bytes\nVersion: %@\nEntries: %lu\nSCE packages: %lu",
        url.lastPathComponent ?: @"PS3UPDAT.PUP",
        static_cast<unsigned long long>(fileSize),
        firmwareVersion ?: @"unknown",
        static_cast<unsigned long>(parsed.entries.size()),
        static_cast<unsigned long>(sceEntries.count)];
    for (NSDictionary *entry in sceEntries) {
        NSDictionary *sce = entry[@"sce"];
        [summary appendFormat:@"\nSCE 0x%llx: metadata %@",
            [entry[@"id"] unsignedLongLongValue],
            [sce[@"metadata_encrypted"] boolValue] ? @"encrypted" : @"plaintext"];
    }
    [summary appendFormat:@"\n%@", manifestSaved ? @"Manifest saved. VSH bootstrap is ready to run."
        : [NSString stringWithFormat:@"Manifest save failed: %@", manifestError.localizedDescription ?: @"unknown error"]];
    self.activePs3Machine[@"last_result"] = summary;
    [self savePs3Machines];
    [self showPs3Detail];
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller
 didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    (void)controller;
    NSURL *url = urls.firstObject;
    if (url == nil || !self.pickingPs3Firmware) return;
    self.pickingPs3Firmware = NO;
    [self inspectPs3FirmwareAtURL:url];
}

@end

@interface VSHiftAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

@implementation VSHiftAppDelegate
- (UIInterfaceOrientationMask)application:(UIApplication *)application
        supportedInterfaceOrientationsForWindow:(UIWindow *)window {
    (void)application;
    UIViewController *presented = window.rootViewController;
    while (presented.presentedViewController != nil) {
        presented = presented.presentedViewController;
    }
    if ([presented isKindOfClass:VSHiftDisplayViewController.class]) {
        return UIInterfaceOrientationMaskLandscape;
    }
    return UIInterfaceOrientationMaskPortrait;
}

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    (void)application;
    (void)launchOptions;
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    VSHiftJITViewController *root = [[VSHiftJITViewController alloc] init];
    UINavigationController *navigation = [[UINavigationController alloc] initWithRootViewController:root];
    navigation.navigationBar.prefersLargeTitles = YES;
    UINavigationBarAppearance *appearance = [[UINavigationBarAppearance alloc] init];
    [appearance configureWithDefaultBackground];
    navigation.navigationBar.standardAppearance = appearance;
    navigation.navigationBar.scrollEdgeAppearance = appearance;
    navigation.navigationBar.compactAppearance = appearance;
    self.window.rootViewController = navigation;
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
