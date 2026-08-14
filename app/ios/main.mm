#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "core/firmware/ps3_package.h"
#include "core/firmware/ps3_pup.h"
#include "core/firmware/ps3_tar.h"
#include "core/loader/ps3_sce.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>

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

@interface VSHiftJITViewController : UIViewController <UIDocumentPickerDelegate>
@property(nonatomic, strong) UIScrollView *scrollView;
@property(nonatomic, strong) UIStackView *contentStack;
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UILabel *guestFrameStateLabel;
@property(nonatomic, strong) UIImageView *guestFrameView;
@property(nonatomic, strong) UIButton *ps3BootButton;
@property(nonatomic, strong) UIButton *exportManifestButton;
@property(nonatomic, strong) NSURL *manifestURL;
@property(nonatomic, strong) NSURL *ps3FirmwareURL;
@property(nonatomic, strong) NSMutableArray<NSMutableDictionary *> *ps3Machines;
@property(nonatomic, strong) NSMutableDictionary *activePs3Machine;
@property(nonatomic, assign) VSHiftPS3Screen ps3Screen;
@property(nonatomic, assign) VSHiftPS3Screen settingsReturnScreen;
@property(nonatomic, assign) BOOL editingPs3Machines;
@property(nonatomic, assign) BOOL pickingPs3Firmware;
@end

@implementation VSHiftJITViewController

static std::uint64_t ReadU64BE(const std::uint8_t *bytes) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = (value << 8) | bytes[index];
    }
    return value;
}

- (UIColor *)cardColor {
    return [UIColor colorWithWhite:0.12 alpha:1.0];
}

- (UIColor *)secondaryCardColor {
    return [UIColor colorWithWhite:0.17 alpha:1.0];
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
    configuration.baseForegroundColor = UIColor.whiteColor;
    if (style == VSHiftButtonStyleFilled) {
        configuration.baseBackgroundColor = [UIColor colorWithWhite:0.24 alpha:1.0];
    }
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    button.configuration = configuration;
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (UIView *)cardContainingStack:(UIStackView *)stack {
    UIView *card = [[UIView alloc] init];
    card.backgroundColor = [self cardColor];
    card.layer.cornerRadius = 24.0;
    card.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = 12.0;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [card addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:card.layoutMarginsGuide.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:card.layoutMarginsGuide.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:card.layoutMarginsGuide.topAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:card.layoutMarginsGuide.bottomAnchor],
    ]];
    return card;
}

- (void)prepareScreen {
    for (UIView *subview in self.view.subviews.copy) {
        [subview removeFromSuperview];
    }
    self.view.backgroundColor = UIColor.blackColor;
    self.view.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;

    self.scrollView = [[UIScrollView alloc] init];
    self.scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.scrollView.alwaysBounceVertical = YES;
    self.scrollView.showsVerticalScrollIndicator = NO;
    [self.view addSubview:self.scrollView];

    self.contentStack = [[UIStackView alloc] init];
    self.contentStack.translatesAutoresizingMaskIntoConstraints = NO;
    self.contentStack.axis = UILayoutConstraintAxisVertical;
    self.contentStack.spacing = 18.0;
    self.contentStack.layoutMargins = UIEdgeInsetsMake(18.0, 20.0, 32.0, 20.0);
    self.contentStack.layoutMarginsRelativeArrangement = YES;
    [self.scrollView addSubview:self.contentStack];

    [NSLayoutConstraint activateConstraints:@[
        [self.scrollView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [self.scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.scrollView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [self.contentStack.leadingAnchor constraintEqualToAnchor:self.scrollView.contentLayoutGuide.leadingAnchor],
        [self.contentStack.trailingAnchor constraintEqualToAnchor:self.scrollView.contentLayoutGuide.trailingAnchor],
        [self.contentStack.topAnchor constraintEqualToAnchor:self.scrollView.contentLayoutGuide.topAnchor],
        [self.contentStack.bottomAnchor constraintEqualToAnchor:self.scrollView.contentLayoutGuide.bottomAnchor],
        [self.contentStack.widthAnchor constraintEqualToAnchor:self.scrollView.frameLayoutGuide.widthAnchor],
    ]];
}

- (void)addSectionTitle:(NSString *)title {
    UILabel *label = [self labelWithText:title
                                    font:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
                                   color:UIColor.secondaryLabelColor];
    [self.contentStack addArrangedSubview:label];
}

- (UIStackView *)topBarWithBackTitle:(NSString *)backTitle
                              action:(SEL)backAction
                           trailing:(NSArray<UIButton *> *)trailing {
    UIButton *backButton = [self buttonWithTitle:backTitle
                                           symbol:@"chevron.left"
                                             style:VSHiftButtonStyleGray
                                           action:backAction];
    UIStackView *bar = [[UIStackView alloc] initWithArrangedSubviews:@[backButton]];
    bar.axis = UILayoutConstraintAxisHorizontal;
    bar.spacing = 10.0;
    for (UIButton *button in trailing) {
        [bar addArrangedSubview:[[UIView alloc] init]];
        [bar addArrangedSubview:button];
    }
    return bar;
}

- (void)addDetailRow:(NSString *)title
               value:(NSString *)value
              symbol:(NSString *)symbol
               stack:(UIStackView *)stack {
    UIImageView *icon = [[UIImageView alloc] initWithImage:[UIImage systemImageNamed:symbol]];
    icon.tintColor = UIColor.whiteColor;
    icon.contentMode = UIViewContentModeScaleAspectFit;
    [icon.widthAnchor constraintEqualToConstant:28.0].active = YES;

    UILabel *name = [self labelWithText:title
                                   font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                                  color:UIColor.whiteColor];
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
    UIButton *addButton = [self buttonWithTitle:@""
                                          symbol:@"plus"
                                            style:VSHiftButtonStyleGray
                                          action:@selector(showPs3Start)];
    UIButton *settingsButton = [self buttonWithTitle:@"Settings"
                                               symbol:@"gearshape"
                                                 style:VSHiftButtonStyleGray
                                               action:@selector(showPs3LibrarySettings)];
    UIButton *editButton = [self buttonWithTitle:self.editingPs3Machines ? @"Done" : @"Edit"
                                            symbol:self.editingPs3Machines ? @"checkmark" : @"pencil"
                                              style:VSHiftButtonStyleGray
                                            action:@selector(togglePs3Editing)];
    UIStackView *topBar = [[UIStackView alloc] initWithArrangedSubviews:@[
        addButton, [[UIView alloc] init], settingsButton, editButton
    ]];
    topBar.axis = UILayoutConstraintAxisHorizontal;
    topBar.spacing = 10.0;
    [self.contentStack addArrangedSubview:topBar];

    UILabel *title = [self labelWithText:@"VSHift PS3"
                                    font:[UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle]
                                   color:UIColor.whiteColor];
    title.font = [UIFont systemFontOfSize:title.font.pointSize weight:UIFontWeightBold];
    [self.contentStack addArrangedSubview:title];

    if (self.ps3Machines.count == 0) {
        UIStackView *emptyStack = [[UIStackView alloc] initWithArrangedSubviews:@[
            [self labelWithText:@"No virtual consoles"
                            font:[UIFont preferredFontForTextStyle:UIFontTextStyleTitle2]
                           color:UIColor.whiteColor],
            [self labelWithText:@"Create a PS3 console profile, then import your own firmware file."
                            font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                           color:UIColor.secondaryLabelColor],
            [self buttonWithTitle:@"Create virtual console"
                           symbol:@"gamecontroller"
                             style:VSHiftButtonStyleFilled
                           action:@selector(createPs3Machine)]
        ]];
        [self.contentStack addArrangedSubview:[self cardContainingStack:emptyStack]];
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
        configuration.baseForegroundColor = UIColor.whiteColor;
        configuration.baseBackgroundColor = [self secondaryCardColor];
        configuration.cornerStyle = UIButtonConfigurationCornerStyleLarge;
        UIButton *machineButton = [UIButton buttonWithType:UIButtonTypeSystem];
        machineButton.configuration = configuration;
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
    [self.contentStack addArrangedSubview:[self topBarWithBackTitle:@"Cancel"
                                                              action:@selector(showPs3Library)
                                                           trailing:@[]]];
    UILabel *title = [self labelWithText:@"Start"
                                    font:[UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle]
                                   color:UIColor.whiteColor];
    title.font = [UIFont systemFontOfSize:title.font.pointSize weight:UIFontWeightBold];
    [self.contentStack addArrangedSubview:title];

    [self addSectionTitle:@"Custom"];
    UIButton *newButton = [UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration *newConfiguration = [UIButtonConfiguration grayButtonConfiguration];
    newConfiguration.title = @"New virtual console";
    newConfiguration.subtitle = @"Create a PS3 console profile from scratch.";
    newConfiguration.image = [UIImage systemImageNamed:@"gamecontroller"];
    newConfiguration.imagePadding = 16.0;
    newConfiguration.baseForegroundColor = UIColor.whiteColor;
    newConfiguration.baseBackgroundColor = [self secondaryCardColor];
    newConfiguration.cornerStyle = UIButtonConfigurationCornerStyleLarge;
    newButton.configuration = newConfiguration;
    [newButton addTarget:self action:@selector(createPs3Machine) forControlEvents:UIControlEventTouchUpInside];
    [self.contentStack addArrangedSubview:newButton];

    [self addSectionTitle:@"Existing"];
    UIButton *importButton = [self buttonWithTitle:@"Import PS3UPDAT.PUP"
                                            symbol:@"doc.badge.plus"
                                              style:VSHiftButtonStyleTinted
                                            action:@selector(createPs3MachineAndImport)];
    [self.contentStack addArrangedSubview:importButton];
}

- (UIView *)displayCard {
    UIView *display = [[UIView alloc] init];
    display.backgroundColor = UIColor.blackColor;
    display.layer.cornerRadius = 24.0;
    display.clipsToBounds = YES;
    self.guestFrameView = [[UIImageView alloc] init];
    self.guestFrameView.translatesAutoresizingMaskIntoConstraints = NO;
    self.guestFrameView.contentMode = UIViewContentModeScaleAspectFit;
    self.guestFrameView.hidden = YES;
    self.guestFrameStateLabel = [self labelWithText:@"PS3 virtual display\nFramebuffer output is not connected yet"
                                               font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                                              color:UIColor.whiteColor];
    self.guestFrameStateLabel.textAlignment = NSTextAlignmentCenter;
    self.guestFrameStateLabel.translatesAutoresizingMaskIntoConstraints = NO;
    UIButtonConfiguration *playConfiguration = [UIButtonConfiguration plainButtonConfiguration];
    playConfiguration.image = [UIImage systemImageNamed:@"play.circle.fill"];
    playConfiguration.preferredSymbolConfigurationForImage =
        [UIImageSymbolConfiguration configurationWithPointSize:88.0 weight:UIImageSymbolWeightRegular];
    playConfiguration.baseForegroundColor = UIColor.whiteColor;
    UIButton *playButton = [UIButton buttonWithType:UIButtonTypeSystem];
    playButton.configuration = playConfiguration;
    [playButton addTarget:self action:@selector(startPs3Machine) forControlEvents:UIControlEventTouchUpInside];
    playButton.translatesAutoresizingMaskIntoConstraints = NO;
    [display addSubview:self.guestFrameView];
    [display addSubview:self.guestFrameStateLabel];
    [display addSubview:playButton];
    [NSLayoutConstraint activateConstraints:@[
        [display.heightAnchor constraintEqualToConstant:224.0],
        [self.guestFrameView.leadingAnchor constraintEqualToAnchor:display.leadingAnchor],
        [self.guestFrameView.trailingAnchor constraintEqualToAnchor:display.trailingAnchor],
        [self.guestFrameView.topAnchor constraintEqualToAnchor:display.topAnchor],
        [self.guestFrameView.bottomAnchor constraintEqualToAnchor:display.bottomAnchor],
        [self.guestFrameStateLabel.leadingAnchor constraintEqualToAnchor:display.leadingAnchor constant:16.0],
        [self.guestFrameStateLabel.trailingAnchor constraintEqualToAnchor:display.trailingAnchor constant:-16.0],
        [self.guestFrameStateLabel.bottomAnchor constraintEqualToAnchor:display.bottomAnchor constant:-16.0],
        [playButton.centerXAnchor constraintEqualToAnchor:display.centerXAnchor],
        [playButton.centerYAnchor constraintEqualToAnchor:display.centerYAnchor],
    ]];
    return display;
}

- (void)showPs3DetailContent {
    UIButton *settings = [self buttonWithTitle:@""
                                         symbol:@"slider.horizontal.3"
                                           style:VSHiftButtonStyleGray
                                         action:@selector(showPs3DetailSettings)];
    [self.contentStack addArrangedSubview:[self topBarWithBackTitle:@""
                                                              action:@selector(showPs3Library)
                                                           trailing:@[settings]]];

    UILabel *title = [self labelWithText:[self machineName:self.activePs3Machine]
                                    font:[UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle]
                                   color:UIColor.whiteColor];
    title.font = [UIFont systemFontOfSize:title.font.pointSize weight:UIFontWeightBold];
    [self.contentStack addArrangedSubview:title];
    [self.contentStack addArrangedSubview:[self displayCard]];

    UIView *detailsCard = [[UIView alloc] init];
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
    statusCard.layer.cornerRadius = 24.0;
    statusCard.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);
    UILabel *statusTitle = [self labelWithText:@"Runtime status" font:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline] color:UIColor.whiteColor];
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

    [self.contentStack addArrangedSubview:[self buttonWithTitle:@"Import PS3UPDAT.PUP" symbol:@"doc.badge.plus" style:VSHiftButtonStyleTinted action:@selector(importPs3Firmware)]];
    self.ps3BootButton = [self buttonWithTitle:@"Prepare VSH bootstrap" symbol:@"play.tv" style:VSHiftButtonStyleFilled action:@selector(preparePs3VshBootstrap)];
    self.ps3BootButton.enabled = self.ps3FirmwareURL != nil;
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
    [self.contentStack addArrangedSubview:[self topBarWithBackTitle:@"" action:backAction trailing:@[]]];
    UILabel *title = [self labelWithText:@"Settings" font:[UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle] color:UIColor.whiteColor];
    title.font = [UIFont systemFontOfSize:title.font.pointSize weight:UIFontWeightBold];
    [self.contentStack addArrangedSubview:title];
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
        configuration.baseForegroundColor = UIColor.whiteColor;
        configuration.baseBackgroundColor = [self cardColor];
        configuration.cornerStyle = UIButtonConfigurationCornerStyleLarge;
        UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
        button.configuration = configuration;
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
    __weak VSHiftJITViewController *weakSelf = self;
    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Create" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        (void)action;
        VSHiftJITViewController *strongSelf = weakSelf;
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
    __weak VSHiftJITViewController *weakSelf = self;
    [alert addAction:[UIAlertAction actionWithTitle:@"Delete" style:UIAlertActionStyleDestructive handler:^(UIAlertAction *action) {
        (void)action;
        VSHiftJITViewController *strongSelf = weakSelf;
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
        self.activePs3Machine[@"last_result"] = @"Import PS3UPDAT.PUP before starting the console.";
        [self savePs3Machines];
        [self rebuildPs3Screen];
        return;
    }
    [self preparePs3VshBootstrap];
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
    NSURL *url = self.ps3FirmwareURL;
    if (url == nil) {
        self.statusLabel.text = @"Import PS3UPDAT.PUP first.";
        return;
    }
    const BOOL accessed = [url startAccessingSecurityScopedResource];
    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:url.path error:nil];
    NSNumber *fileSizeNumber = attributes[NSFileSize];
    std::uint64_t fileSize = fileSizeNumber != nil ? fileSizeNumber.unsignedLongLongValue : 0;
    if (fileSize < vshift::firmware::kPs3PupHeaderSize ||
        fileSize > static_cast<std::uint64_t>(std::numeric_limits<NSUInteger>::max())) {
        self.statusLabel.text = @"PS3 PUP is too small or too large to read.";
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    NSData *pupData = [NSData dataWithContentsOfURL:url options:NSDataReadingMappedIfSafe error:nil];
    if (pupData.length != static_cast<NSUInteger>(fileSize)) {
        self.statusLabel.text = @"Could not read PS3UPDAT.PUP.";
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto *pupBytes = static_cast<const std::uint8_t *>(pupData.bytes);
    const auto headerLength = ReadU64BE(pupBytes + 0x20);
    if (headerLength < vshift::firmware::kPs3PupHeaderSize ||
        headerLength > 16ull * 1024ull * 1024ull || headerLength > fileSize) {
        self.statusLabel.text = @"PS3 PUP header size is invalid.";
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto parsed = vshift::firmware::ParsePs3PupHeaders(
        std::span<const std::uint8_t>(pupBytes, static_cast<std::size_t>(headerLength)), fileSize);
    if (!parsed.ok()) {
        self.statusLabel.text = [NSString stringWithFormat:@"PS3 PUP parse failed:\n%s", parsed.error.c_str()];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto updateEntry = std::find_if(parsed.entries.begin(), parsed.entries.end(), [](const auto &entry) {
        return entry.entry_id == 0x300;
    });
    if (updateEntry == parsed.entries.end() ||
        updateEntry->data_offset > pupData.length ||
        updateEntry->data_length > pupData.length - static_cast<NSUInteger>(updateEntry->data_offset)) {
        self.statusLabel.text = @"PS3 PUP has no readable update TAR.";
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto *updateTarBytes = pupBytes + static_cast<std::size_t>(updateEntry->data_offset);
    const auto updateTar = vshift::firmware::ParsePs3Tar(
        std::span<const std::uint8_t>(updateTarBytes, static_cast<std::size_t>(updateEntry->data_length)));
    if (!updateTar.ok()) {
        self.statusLabel.text = [NSString stringWithFormat:@"PS3 update TAR parse failed:\n%s", updateTar.error.c_str()];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto packageEntry = std::find_if(updateTar.entries.begin(), updateTar.entries.end(), [](const auto &entry) {
        return entry.regular_file && entry.name.rfind("dev_flash_012", 0) == 0;
    });
    if (packageEntry == updateTar.entries.end()) {
        self.statusLabel.text = @"dev_flash_012 package is missing.";
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    const auto packageBegin = static_cast<std::size_t>(packageEntry->data_offset);
    const auto packageLength = static_cast<std::size_t>(packageEntry->data_length);
    const auto package = vshift::firmware::DecryptPs3ScePackage(
        std::span<const std::uint8_t>(updateTarBytes + packageBegin, packageLength));
    if (!package.ok()) {
        self.statusLabel.text = [NSString stringWithFormat:@"dev_flash_012 decrypt failed:\n%s", package.error.c_str()];
        if (accessed) [url stopAccessingSecurityScopedResource];
        return;
    }
    std::uint64_t vshSize = 0;
    for (const auto &section : package.sections) {
        const auto candidate = vshift::firmware::ParsePs3Tar(section.bytes);
        if (!candidate.ok()) continue;
        const auto found = std::find_if(candidate.entries.begin(), candidate.entries.end(), [](const auto &entry) {
            return entry.regular_file && entry.name == "dev_flash/vsh/module/vsh.self";
        });
        if (found != candidate.entries.end()) {
            vshSize = found->data_length;
            break;
        }
    }
    if (accessed) [url stopAccessingSecurityScopedResource];
    if (vshSize == 0) {
        self.statusLabel.text = @"dev_flash_012 decrypted, but vsh.self was not found.";
        return;
    }

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
        },
        @"boot_status": @"dev_flash package decrypted; PPU/LV2/RSX execution pending",
    };
    const BOOL manifestSaved = [self writeManifest:manifest error:&manifestError];
    self.activePs3Machine[@"status"] = @"VSH bootstrap ready";
    self.activePs3Machine[@"vsh_self_size"] = @(vshSize);
    self.activePs3Machine[@"last_result"] = [NSString stringWithFormat:
        @"VSH bootstrap ready\nFirmware: %@\nPackage: %@\nDecrypted sections: %lu\nvsh.self: %llu bytes\n%@",
        version,
        [NSString stringWithUTF8String:packageEntry->name.c_str()],
        static_cast<unsigned long>(package.sections.size()),
        static_cast<unsigned long long>(vshSize),
        manifestSaved ? @"Next: PPU/LV2/RSX runtime and framebuffer."
                      : [NSString stringWithFormat:@"Manifest save failed: %@", manifestError.localizedDescription ?: @"unknown error"]];
    [self savePs3Machines];
    [self showPs3Detail];
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
