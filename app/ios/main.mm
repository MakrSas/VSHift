#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "core/boot/synthetic_boot.h"
#include "core/cpu/arm64_jit.h"
#include "core/firmware/catalog.h"
#include "core/firmware/pup.h"
#include "core/firmware/slb2.h"
#include "core/cpu/x86_decoder.h"
#include "core/loader/self.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

@interface VSHiftPS4ScreenViewController : UIViewController
@property(nonatomic, strong) NSURL *firmwareRootURL;
@property(nonatomic, copy) NSString *firmwareName;
@property(nonatomic, copy) NSString *shellStatus;
@end

@interface VSHiftJITViewController : UIViewController <UIDocumentPickerDelegate>
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UIButton *exportManifestButton;
@property(nonatomic, strong) NSURL *manifestURL;
@property(nonatomic, strong) NSURL *firmwareRootURL;
@property(nonatomic, strong) UIButton *launchScreenButton;
@property(nonatomic, assign) BOOL pickingDecryptedFirmware;
@property(nonatomic, assign) BOOL pickingFirmwareRoot;
@property(nonatomic, assign) BOOL pickingFirmwareSelfProbe;
@end

@implementation VSHiftPS4ScreenViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.blackColor;

    UIScrollView *scrollView = [[UIScrollView alloc] init];
    scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    scrollView.alwaysBounceVertical = YES;
    scrollView.showsVerticalScrollIndicator = NO;
    [self.view addSubview:scrollView];

    UIStackView *content = [[UIStackView alloc] init];
    content.translatesAutoresizingMaskIntoConstraints = NO;
    content.axis = UILayoutConstraintAxisVertical;
    content.spacing = 18.0;
    content.layoutMargins = UIEdgeInsetsMake(22.0, 20.0, 28.0, 20.0);
    content.layoutMarginsRelativeArrangement = YES;
    [scrollView addSubview:content];

    UIButtonConfiguration *closeConfiguration =
        [UIButtonConfiguration grayButtonConfiguration];
    closeConfiguration.title = @"Done";
    closeConfiguration.image = [UIImage systemImageNamed:@"xmark"];
    closeConfiguration.imagePadding = 6.0;
    closeConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    UIButton *closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
    closeButton.configuration = closeConfiguration;
    [closeButton addTarget:self action:@selector(closeScreen)
          forControlEvents:UIControlEventTouchUpInside];

    UILabel *eyebrow = [[UILabel alloc] init];
    eyebrow.text = @"PLAYSTATION 4 · VSHIFT";
    eyebrow.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    eyebrow.textColor = UIColor.systemGray2Color;

    UILabel *title = [[UILabel alloc] init];
    title.text = @"PS4 VSH";
    title.font = [UIFont systemFontOfSize:42.0 weight:UIFontWeightSemibold];
    title.textColor = UIColor.whiteColor;

    UILabel *subtitle = [[UILabel alloc] init];
    subtitle.text = self.firmwareName.length > 0
        ? [NSString stringWithFormat:@"%@\n%@", self.firmwareName,
           self.shellStatus ?: @"SceShellCore metadata mapped"]
        : (self.shellStatus ?: @"SceShellCore metadata mapped");
    subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    subtitle.textColor = UIColor.systemGray2Color;
    subtitle.numberOfLines = 0;

    UIStackView *heading = [[UIStackView alloc] initWithArrangedSubviews:@[
        closeButton, eyebrow, title, subtitle
    ]];
    heading.axis = UILayoutConstraintAxisVertical;
    heading.spacing = 8.0;
    [content addArrangedSubview:heading];

    NSArray<NSString *> *tabs = @[@"Games", @"Media", @"Settings"];
    UIStackView *tabStack = [[UIStackView alloc] init];
    tabStack.axis = UILayoutConstraintAxisHorizontal;
    tabStack.spacing = 10.0;
    tabStack.distribution = UIStackViewDistributionFillEqually;
    for (NSString *tab in tabs) {
        UIButtonConfiguration *tabConfiguration =
            [UIButtonConfiguration tintedButtonConfiguration];
        tabConfiguration.title = tab;
        tabConfiguration.baseForegroundColor = UIColor.systemGray2Color;
        tabConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
        UIButton *tabButton = [UIButton buttonWithType:UIButtonTypeSystem];
        tabButton.configuration = tabConfiguration;
        [tabStack addArrangedSubview:tabButton];
    }
    [content addArrangedSubview:tabStack];

    UILabel *section = [[UILabel alloc] init];
    section.text = @"What’s new";
    section.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle3];
    section.textColor = UIColor.whiteColor;
    [content addArrangedSubview:section];

    UIView *welcomeCard = [[UIView alloc] init];
    welcomeCard.backgroundColor = [UIColor colorWithWhite:0.12 alpha:1.0];
    welcomeCard.layer.cornerRadius = 24.0;
    welcomeCard.layoutMargins = UIEdgeInsetsMake(18.0, 18.0, 18.0, 18.0);
    UILabel *welcomeTitle = [[UILabel alloc] init];
    welcomeTitle.text = @"Firmware workspace ready";
    welcomeTitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    welcomeTitle.textColor = UIColor.whiteColor;
    UILabel *welcomeBody = [[UILabel alloc] init];
    welcomeBody.text = @"The selected PS4 system root is mounted for the VSHift HLE display. The next guest milestone is executing the protected SELF payload through the x86-64 translator.";
    welcomeBody.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    welcomeBody.textColor = UIColor.systemGray2Color;
    welcomeBody.numberOfLines = 0;
    UIStackView *welcomeStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        welcomeTitle, welcomeBody
    ]];
    welcomeStack.axis = UILayoutConstraintAxisVertical;
    welcomeStack.spacing = 8.0;
    welcomeStack.translatesAutoresizingMaskIntoConstraints = NO;
    [welcomeCard addSubview:welcomeStack];
    [NSLayoutConstraint activateConstraints:@[
        [welcomeStack.leadingAnchor constraintEqualToAnchor:welcomeCard.layoutMarginsGuide.leadingAnchor],
        [welcomeStack.trailingAnchor constraintEqualToAnchor:welcomeCard.layoutMarginsGuide.trailingAnchor],
        [welcomeStack.topAnchor constraintEqualToAnchor:welcomeCard.layoutMarginsGuide.topAnchor],
        [welcomeStack.bottomAnchor constraintEqualToAnchor:welcomeCard.layoutMarginsGuide.bottomAnchor],
    ]];
    [content addArrangedSubview:welcomeCard];

    UIStackView *tiles = [[UIStackView alloc] init];
    tiles.axis = UILayoutConstraintAxisHorizontal;
    tiles.spacing = 12.0;
    tiles.distribution = UIStackViewDistributionFillEqually;
    NSArray<NSArray<NSString *> *> *tileData = @[
        @[@"🎮", @"Games"],
        @[@"🌐", @"Web Browser"],
        @[@"⚙︎", @"Settings"],
    ];
    for (NSArray<NSString *> *data in tileData) {
        UIView *tile = [[UIView alloc] init];
        tile.backgroundColor = [UIColor colorWithWhite:0.10 alpha:1.0];
        tile.layer.cornerRadius = 20.0;
        tile.layoutMargins = UIEdgeInsetsMake(16.0, 12.0, 16.0, 12.0);
        UILabel *icon = [[UILabel alloc] init];
        icon.text = data[0];
        icon.font = [UIFont systemFontOfSize:30.0];
        icon.textAlignment = NSTextAlignmentCenter;
        UILabel *name = [[UILabel alloc] init];
        name.text = data[1];
        name.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
        name.textColor = UIColor.systemGray2Color;
        name.textAlignment = NSTextAlignmentCenter;
        UIStackView *tileStack = [[UIStackView alloc] initWithArrangedSubviews:@[
            icon, name
        ]];
        tileStack.axis = UILayoutConstraintAxisVertical;
        tileStack.spacing = 8.0;
        tileStack.translatesAutoresizingMaskIntoConstraints = NO;
        [tile addSubview:tileStack];
        [NSLayoutConstraint activateConstraints:@[
            [tileStack.leadingAnchor constraintEqualToAnchor:tile.layoutMarginsGuide.leadingAnchor],
            [tileStack.trailingAnchor constraintEqualToAnchor:tile.layoutMarginsGuide.trailingAnchor],
            [tileStack.topAnchor constraintEqualToAnchor:tile.layoutMarginsGuide.topAnchor],
            [tileStack.bottomAnchor constraintEqualToAnchor:tile.layoutMarginsGuide.bottomAnchor],
        ]];
        [tiles addArrangedSubview:tile];
    }
    [content addArrangedSubview:tiles];

    UILabel *footer = [[UILabel alloc] init];
    footer.text = @"VSHift · PS4 screen output\nReal SELF headers: validated · Guest code: next stage";
    footer.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    footer.textColor = UIColor.systemGrayColor;
    footer.numberOfLines = 0;
    [content addArrangedSubview:footer];

    [NSLayoutConstraint activateConstraints:@[
        [scrollView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [scrollView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [content.leadingAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.leadingAnchor],
        [content.trailingAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.trailingAnchor],
        [content.topAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor],
        [content.bottomAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor],
        [content.widthAnchor constraintEqualToAnchor:scrollView.frameLayoutGuide.widthAnchor],
    ]];
}

- (void)closeScreen {
    [self dismissViewControllerAnimated:YES completion:nil];
}

@end

@implementation VSHiftJITViewController

static std::uint32_t ReadU32LE(const std::uint8_t *bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

static std::uint16_t ReadU16LE(const std::uint8_t *bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           (static_cast<std::uint16_t>(bytes[1]) << 8);
}

- (void)viewDidLoad {
    [super viewDidLoad];

    NSString *message =
        @"PS4 VSH root is manual. Choose an extracted root, then run the boot test.";

    self.view.backgroundColor = UIColor.systemGroupedBackgroundColor;

    UIScrollView *scrollView = [[UIScrollView alloc] init];
    scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    scrollView.alwaysBounceVertical = YES;
    scrollView.showsVerticalScrollIndicator = NO;
    [self.view addSubview:scrollView];

    UIStackView *content = [[UIStackView alloc] init];
    content.translatesAutoresizingMaskIntoConstraints = NO;
    content.axis = UILayoutConstraintAxisVertical;
    content.spacing = 20.0;
    content.layoutMargins = UIEdgeInsetsMake(24.0, 20.0, 32.0, 20.0);
    content.layoutMarginsRelativeArrangement = YES;
    [scrollView addSubview:content];

    UILabel *eyebrow = [[UILabel alloc] init];
    eyebrow.text = @"VSHIFT LAB";
    eyebrow.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    eyebrow.textColor = UIColor.secondaryLabelColor;
    eyebrow.adjustsFontForContentSizeCategory = YES;

    UILabel *title = [[UILabel alloc] init];
    title.text = @"PS4 firmware workspace";
    title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];
    title.textColor = UIColor.labelColor;
    title.adjustsFontForContentSizeCategory = YES;

    UILabel *subtitle = [[UILabel alloc] init];
    subtitle.text = @"Validate an extracted PS4 firmware root and VSH boot path.";
    subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    subtitle.textColor = UIColor.secondaryLabelColor;
    subtitle.numberOfLines = 0;
    subtitle.adjustsFontForContentSizeCategory = YES;

    UIStackView *header = [[UIStackView alloc] initWithArrangedSubviews:@[
        eyebrow, title, subtitle
    ]];
    header.axis = UILayoutConstraintAxisVertical;
    header.spacing = 4.0;

    UIView *statusCard = [[UIView alloc] init];
    statusCard.backgroundColor = UIColor.secondarySystemBackgroundColor;
    statusCard.layer.cornerRadius = 24.0;
    statusCard.layoutMargins = UIEdgeInsetsMake(18.0, 18.0, 18.0, 18.0);

    UILabel *statusTitle = [[UILabel alloc] init];
    statusTitle.text = @"Runtime status";
    statusTitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    statusTitle.adjustsFontForContentSizeCategory = YES;

    self.statusLabel = [[UILabel alloc] init];
    self.statusLabel.text = message;
    self.statusLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    self.statusLabel.textColor = UIColor.secondaryLabelColor;
    self.statusLabel.numberOfLines = 0;
    self.statusLabel.adjustsFontForContentSizeCategory = YES;

    UIStackView *statusStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        statusTitle, self.statusLabel
    ]];
    statusStack.axis = UILayoutConstraintAxisVertical;
    statusStack.spacing = 8.0;
    statusStack.translatesAutoresizingMaskIntoConstraints = NO;
    [statusCard addSubview:statusStack];

    UILabel *firmwareHeader = [[UILabel alloc] init];
    firmwareHeader.text = @"FIRMWARE";
    firmwareHeader.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    firmwareHeader.textColor = UIColor.secondaryLabelColor;
    firmwareHeader.adjustsFontForContentSizeCategory = YES;

    UIButtonConfiguration *rootConfiguration =
        [UIButtonConfiguration filledButtonConfiguration];
    rootConfiguration.title = @"Import extracted PS4 firmware root";
    rootConfiguration.image = [UIImage systemImageNamed:@"folder.badge.gearshape"];
    rootConfiguration.imagePadding = 8.0;
    rootConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    UIButton *rootButton = [UIButton buttonWithType:UIButtonTypeSystem];
    rootButton.configuration = rootConfiguration;
    [rootButton addTarget:self
                   action:@selector(importFirmwareRoot)
         forControlEvents:UIControlEventTouchUpInside];

    UIButtonConfiguration *exportConfiguration =
        [UIButtonConfiguration tintedButtonConfiguration];
    exportConfiguration.title = @"Export manifest to Files";
    exportConfiguration.image = [UIImage systemImageNamed:@"square.and.arrow.up"];
    exportConfiguration.imagePadding = 8.0;
    exportConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    self.exportManifestButton = [UIButton buttonWithType:UIButtonTypeSystem];
    self.exportManifestButton.configuration = exportConfiguration;
    self.exportManifestButton.enabled = NO;
    [self.exportManifestButton addTarget:self
                                  action:@selector(exportManifest)
                        forControlEvents:UIControlEventTouchUpInside];

    UIView *firmwareCard = [[UIView alloc] init];
    firmwareCard.backgroundColor = UIColor.secondarySystemBackgroundColor;
    firmwareCard.layer.cornerRadius = 24.0;
    firmwareCard.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);
    UIStackView *firmwareStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        firmwareHeader, rootButton,
        self.exportManifestButton
    ]];
    firmwareStack.axis = UILayoutConstraintAxisVertical;
    firmwareStack.spacing = 12.0;
    firmwareStack.translatesAutoresizingMaskIntoConstraints = NO;
    [firmwareCard addSubview:firmwareStack];

    UILabel *bootHeader = [[UILabel alloc] init];
    bootHeader.text = @"BOOT LAB";
    bootHeader.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    bootHeader.textColor = UIColor.secondaryLabelColor;
    bootHeader.adjustsFontForContentSizeCategory = YES;

    UIButtonConfiguration *jitConfiguration =
        [UIButtonConfiguration filledButtonConfiguration];
    jitConfiguration.title = @"Run synthetic boot · JIT";
    jitConfiguration.image = [UIImage systemImageNamed:@"bolt.fill"];
    jitConfiguration.imagePadding = 8.0;
    jitConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    UIButton *syntheticJitButton = [UIButton buttonWithType:UIButtonTypeSystem];
    syntheticJitButton.configuration = jitConfiguration;
    [syntheticJitButton addTarget:self
                           action:@selector(runSyntheticJit)
                 forControlEvents:UIControlEventTouchUpInside];

    UIButtonConfiguration *jitLessConfiguration =
        [UIButtonConfiguration grayButtonConfiguration];
    jitLessConfiguration.title = @"Run synthetic boot · JIT-less";
    jitLessConfiguration.image = [UIImage systemImageNamed:@"gauge.with.dots.needle.33percent"];
    jitLessConfiguration.imagePadding = 8.0;
    jitLessConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    UIButton *syntheticJitLessButton = [UIButton buttonWithType:UIButtonTypeSystem];
    syntheticJitLessButton.configuration = jitLessConfiguration;
    [syntheticJitLessButton addTarget:self
                               action:@selector(runSyntheticJitLess)
                     forControlEvents:UIControlEventTouchUpInside];

    UIButtonConfiguration *selfProbeConfiguration =
        [UIButtonConfiguration tintedButtonConfiguration];
    selfProbeConfiguration.title = @"Probe real PS4 SELF headers";
    selfProbeConfiguration.image = [UIImage systemImageNamed:@"doc.text.magnifyingglass"];
    selfProbeConfiguration.imagePadding = 8.0;
    selfProbeConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    UIButton *selfProbeButton = [UIButton buttonWithType:UIButtonTypeSystem];
    selfProbeButton.configuration = selfProbeConfiguration;
    [selfProbeButton addTarget:self
                        action:@selector(probeFirmwareSelf)
              forControlEvents:UIControlEventTouchUpInside];

    UIButtonConfiguration *launchConfiguration =
        [UIButtonConfiguration filledButtonConfiguration];
    launchConfiguration.title = @"Launch PS4 screen";
    launchConfiguration.image = [UIImage systemImageNamed:@"tv"];
    launchConfiguration.imagePadding = 8.0;
    launchConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    self.launchScreenButton = [UIButton buttonWithType:UIButtonTypeSystem];
    self.launchScreenButton.configuration = launchConfiguration;
    [self.launchScreenButton addTarget:self
                                action:@selector(launchPS4Screen)
                      forControlEvents:UIControlEventTouchUpInside];

    UIView *bootCard = [[UIView alloc] init];
    bootCard.backgroundColor = UIColor.secondarySystemBackgroundColor;
    bootCard.layer.cornerRadius = 24.0;
    bootCard.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);
    UIStackView *bootStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        bootHeader, self.launchScreenButton, selfProbeButton,
        syntheticJitButton, syntheticJitLessButton
    ]];
    bootStack.axis = UILayoutConstraintAxisVertical;
    bootStack.spacing = 12.0;
    bootStack.translatesAutoresizingMaskIntoConstraints = NO;
    [bootCard addSubview:bootStack];

    [content addArrangedSubview:header];
    [content addArrangedSubview:statusCard];
    [content addArrangedSubview:firmwareCard];
    [content addArrangedSubview:bootCard];

    [NSLayoutConstraint activateConstraints:@[
        [scrollView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [scrollView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [content.leadingAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.leadingAnchor],
        [content.trailingAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.trailingAnchor],
        [content.topAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor],
        [content.bottomAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor],
        [content.widthAnchor constraintEqualToAnchor:scrollView.frameLayoutGuide.widthAnchor],
        [statusStack.leadingAnchor constraintEqualToAnchor:statusCard.layoutMarginsGuide.leadingAnchor],
        [statusStack.trailingAnchor constraintEqualToAnchor:statusCard.layoutMarginsGuide.trailingAnchor],
        [statusStack.topAnchor constraintEqualToAnchor:statusCard.layoutMarginsGuide.topAnchor],
        [statusStack.bottomAnchor constraintEqualToAnchor:statusCard.layoutMarginsGuide.bottomAnchor],
        [firmwareStack.leadingAnchor constraintEqualToAnchor:firmwareCard.layoutMarginsGuide.leadingAnchor],
        [firmwareStack.trailingAnchor constraintEqualToAnchor:firmwareCard.layoutMarginsGuide.trailingAnchor],
        [firmwareStack.topAnchor constraintEqualToAnchor:firmwareCard.layoutMarginsGuide.topAnchor],
        [firmwareStack.bottomAnchor constraintEqualToAnchor:firmwareCard.layoutMarginsGuide.bottomAnchor],
        [bootStack.leadingAnchor constraintEqualToAnchor:bootCard.layoutMarginsGuide.leadingAnchor],
        [bootStack.trailingAnchor constraintEqualToAnchor:bootCard.layoutMarginsGuide.trailingAnchor],
        [bootStack.topAnchor constraintEqualToAnchor:bootCard.layoutMarginsGuide.topAnchor],
        [bootStack.bottomAnchor constraintEqualToAnchor:bootCard.layoutMarginsGuide.bottomAnchor],
    ]];
}

- (void)importFirmware {
    self.pickingDecryptedFirmware = NO;
    self.pickingFirmwareRoot = NO;
    self.pickingFirmwareSelfProbe = NO;
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeData]
            asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)importDecryptedFirmware {
    self.pickingDecryptedFirmware = YES;
    self.pickingFirmwareRoot = NO;
    self.pickingFirmwareSelfProbe = NO;
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeData]
                                 asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)importFirmwareRoot {
    self.pickingDecryptedFirmware = NO;
    self.pickingFirmwareRoot = YES;
    self.pickingFirmwareSelfProbe = NO;
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeFolder]
                                 asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)probeFirmwareSelf {
    self.pickingDecryptedFirmware = NO;
    self.pickingFirmwareRoot = NO;
    self.pickingFirmwareSelfProbe = YES;
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeFolder]
                                 asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)launchPS4Screen {
    if (self.firmwareRootURL == nil) {
        self.statusLabel.text =
            @"Choose Firmware 5.05 root first, then press Launch PS4 screen.";
        [self importFirmwareRoot];
        return;
    }

    const BOOL accessed =
        [self.firmwareRootURL startAccessingSecurityScopedResource];
    NSURL *shellURL = [self.firmwareRootURL
        URLByAppendingPathComponent:@"system/vsh/SceShellCore.elf"];
    NSDictionary *attributes = [[NSFileManager defaultManager]
        attributesOfItemAtPath:shellURL.path error:nil];
    NSNumber *size = attributes[NSFileSize];
    NSString *shellStatus = size != nil
        ? [NSString stringWithFormat:
              @"SceShellCore SELF mapped · %@ bytes · HLE display",
              size]
        : @"SceShellCore metadata mapped · HLE display";

    VSHiftPS4ScreenViewController *screen =
        [[VSHiftPS4ScreenViewController alloc] init];
    screen.firmwareRootURL = self.firmwareRootURL;
    screen.firmwareName = self.firmwareRootURL.lastPathComponent;
    screen.shellStatus = shellStatus;
    screen.modalPresentationStyle = UIModalPresentationFullScreen;
    if (accessed) {
        [self.firmwareRootURL stopAccessingSecurityScopedResource];
    }
    [self presentViewController:screen animated:YES completion:nil];
}

- (void)exportManifest {
    if (self.manifestURL == nil) {
        self.statusLabel.text =
            @"Import a firmware file first to create a manifest.";
        return;
    }

    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc]
            initForExportingURLs:@[self.manifestURL]
                           asCopy:YES];
    picker.delegate = self;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)runSyntheticJit {
    [self runSyntheticBootWithMode:vshift::boot::ExecutionMode::Jit];
}

- (void)runSyntheticJitLess {
    [self runSyntheticBootWithMode:vshift::boot::ExecutionMode::JitLess];
}

- (void)runSyntheticBootWithMode:(vshift::boot::ExecutionMode)mode {
    const auto image = vshift::boot::BuildSyntheticElfFixture();
    const auto report = vshift::boot::RunSyntheticElfBoot(image, mode);
    NSString *modeName = mode == vshift::boot::ExecutionMode::Jit
                             ? @"JIT"
                             : @"JIT-less";
    if (!report.ok()) {
        self.statusLabel.text = [NSString stringWithFormat:
            @"SYNTHETIC BOOT FAILED (%@)\n%s", modeName,
            report.error.c_str()];
        return;
    }

    self.statusLabel.text = [NSString stringWithFormat:
        @"BOOT OK\nSynthetic ELF\nMode: %@\nEntry: 0x%llx\nSegments: %d\nResult: %u",
        modeName,
        static_cast<unsigned long long>(report.entry),
        static_cast<int>(report.mapped_segments),
        report.result];
}

- (void)inspectFirmwareRootAtURL:(NSURL *)url {
    self.firmwareRootURL = url;
    const BOOL accessed = [url startAccessingSecurityScopedResource];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSArray<NSString *> *requiredPaths = @[
        @"system/common/lib/libkernel_sys.sprx",
        @"system/common/lib/libSceLibcInternal.sprx",
        @"system/common/lib/libkernel.sprx",
    ];
    NSArray<NSString *> *bootEntryCandidates = @[
        // The PS4 system root stores the system core under /system/sys.
        @"system/sys/SceSysCore.elf",
    ];
    NSArray<NSString *> *optionalPaths = @[
        @"system/vsh/SceShellCore.elf",
        @"system_ex",
        @"preinst",
    ];
    NSMutableArray *requiredFound = [NSMutableArray array];
    NSMutableArray *bootEntryFound = [NSMutableArray array];
    NSMutableArray *optionalFound = [NSMutableArray array];
    NSMutableArray *missingRequired = [NSMutableArray array];
    NSMutableArray *missingBootEntries = [NSMutableArray array];
    NSMutableArray *missingOptional = [NSMutableArray array];
    void (^inspectPath)(NSString *, NSMutableArray *, NSMutableArray *) =
        ^(NSString *relativePath, NSMutableArray *found, NSMutableArray *missing) {
        NSURL *candidate = [url URLByAppendingPathComponent:relativePath];
        BOOL isDirectory = NO;
        const BOOL exists = [fileManager fileExistsAtPath:candidate.path
                                             isDirectory:&isDirectory];
        if (exists) {
            NSDictionary *attributes =
                [fileManager attributesOfItemAtPath:candidate.path error:nil];
            [found addObject:@{
                @"path": relativePath,
                @"kind": isDirectory ? @"directory" : @"file",
                @"size": attributes[NSFileSize] ?: @0,
            }];
        } else {
            [missing addObject:relativePath];
        }
    };
    for (NSString *relativePath in requiredPaths) {
        inspectPath(relativePath, requiredFound, missingRequired);
    }
    for (NSString *relativePath in bootEntryCandidates) {
        inspectPath(relativePath, bootEntryFound, missingBootEntries);
    }
    for (NSString *relativePath in optionalPaths) {
        inspectPath(relativePath, optionalFound, missingOptional);
    }
    NSMutableArray *found = [requiredFound mutableCopy];
    [found addObjectsFromArray:bootEntryFound];
    [found addObjectsFromArray:optionalFound];
    const BOOL requiredPathsPresent =
        missingRequired.count == 0 && bootEntryFound.count > 0;

    NSDictionary *manifest = @{
        @"schema_version": @3,
        @"source_file_name": url.lastPathComponent ?: @"firmware-root",
        @"source_kind": @"user-provided decrypted PS4 firmware root",
        @"boot_profile": @"PS4 VSH preflight",
        @"root_path": url.path ?: @"",
        @"required_paths": requiredPaths,
        @"boot_entry_candidates": bootEntryCandidates,
        @"optional_paths": optionalPaths,
        @"found": found,
        @"required_found": requiredFound,
        @"boot_entry_found": bootEntryFound,
        @"optional_found": optionalFound,
        @"missing_required": missingRequired,
        @"missing_boot_entries": missingBootEntries,
        @"missing_optional": missingOptional,
        @"boot_status": requiredPathsPresent
            ? @"PS4 firmware root preflight passed; guest execution pending"
            : @"Safe Mode root is incomplete",
    };

    if (accessed) {
        [url stopAccessingSecurityScopedResource];
    }

    NSError *manifestError = nil;
    const BOOL manifestSaved = [self writeManifest:manifest error:&manifestError];
    NSMutableString *summary = [NSMutableString stringWithFormat:
        @"FIRMWARE ROOT\n%@\nFound: %lu/%lu required paths",
        url.lastPathComponent ?: @"firmware-root",
        static_cast<unsigned long>(requiredFound.count),
        static_cast<unsigned long>(requiredPaths.count)];
    for (NSDictionary *item in requiredFound) {
        [summary appendFormat:@"\n✓ %@", item[@"path"]];
    }
    if (missingRequired.count > 0) {
        [summary appendFormat:@"\nMissing required: %@",
            [missingRequired componentsJoinedByString:@", "]];
    }
    if (bootEntryFound.count > 0) {
        for (NSDictionary *item in bootEntryFound) {
            [summary appendFormat:@"\n✓ Boot entry: %@", item[@"path"]];
        }
    } else {
        [summary appendFormat:@"\nMissing boot entry: %@",
            [bootEntryCandidates componentsJoinedByString:@" or "]];
    }
    if (missingOptional.count > 0) {
        [summary appendFormat:@"\nOptional not present: %@",
            [missingOptional componentsJoinedByString:@", "]];
    }
    [summary appendFormat:@"\n%@\n%@",
        requiredPathsPresent
            ? @"PS4 firmware root preflight passed; next stage is SELF payload mapping"
            : @"Safe Mode preflight incomplete; choose the extracted firmware root",
        manifestSaved
            ? @"Manifest saved."
            : [NSString stringWithFormat:@"Manifest save failed: %@",
                manifestError.localizedDescription ?: @"unknown error"]];
    self.statusLabel.text = summary;
}

- (void)probeFirmwareSelfAtURL:(NSURL *)url {
    const BOOL accessed = [url startAccessingSecurityScopedResource];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSArray<NSString *> *paths = @[
        @"system/sys/SceSysCore.elf",
        @"system/vsh/SceShellCore.elf",
    ];
    constexpr std::uint16_t kMaximumSelfHeaderSize = 16 * 1024;
    NSMutableArray *probes = [NSMutableArray arrayWithCapacity:paths.count];
    NSMutableString *summary = [NSMutableString stringWithFormat:
        @"PS4 SELF PROBE\n%@", url.lastPathComponent ?: @"firmware-root"];
    std::size_t validCount = 0;

    for (NSString *relativePath in paths) {
        NSURL *candidate = [url URLByAppendingPathComponent:relativePath];
        NSDictionary *attributes =
            [fileManager attributesOfItemAtPath:candidate.path error:nil];
        NSNumber *fileSizeNumber = attributes[NSFileSize];
        const auto fileSize = fileSizeNumber != nil
                                  ? fileSizeNumber.unsignedLongLongValue
                                  : 0;
        NSMutableDictionary *probe = [@{
            @"path": relativePath,
            @"file_size": @(fileSize),
        } mutableCopy];
        if (fileSize < vshift::loader::kSelfHeaderSize) {
            probe[@"error"] = @"file is missing or SELF header is truncated";
            [probes addObject:probe];
            [summary appendFormat:@"\n✗ %@", relativePath];
            continue;
        }

        NSFileHandle *handle =
            [NSFileHandle fileHandleForReadingFromURL:candidate error:nil];
        if (handle == nil) {
            probe[@"error"] = @"could not open SELF";
            [probes addObject:probe];
            [summary appendFormat:@"\n✗ %@ (open failed)", relativePath];
            continue;
        }

        NSData *fixedData =
            [handle readDataOfLength:vshift::loader::kSelfHeaderSize];
        if (fixedData.length < vshift::loader::kSelfHeaderSize) {
            [handle closeFile];
            probe[@"error"] = @"SELF fixed header is truncated";
            [probes addObject:probe];
            [summary appendFormat:@"\n✗ %@ (truncated)", relativePath];
            continue;
        }

        const auto *fixedBytes =
            static_cast<const std::uint8_t *>(fixedData.bytes);
        const auto magic = ReadU32LE(fixedBytes);
        const auto headerSize = ReadU16LE(fixedBytes + 0x0C);
        probe[@"magic"] = [NSString stringWithFormat:@"0x%08x", magic];
        probe[@"header_size"] = @(headerSize);
        if (magic != vshift::loader::kPs4SelfMagic ||
            headerSize < vshift::loader::kSelfHeaderSize ||
            headerSize > kMaximumSelfHeaderSize ||
            headerSize > fileSize) {
            [handle closeFile];
            probe[@"error"] = @"invalid PS4 SELF public header";
            [probes addObject:probe];
            [summary appendFormat:@"\n✗ %@ (not a valid PS4 SELF header)",
                relativePath];
            continue;
        }

        [handle seekToFileOffset:0];
        NSData *headerData =
            [handle readDataOfLength:static_cast<NSUInteger>(headerSize)];
        [handle closeFile];
        if (headerData.length < headerSize) {
            probe[@"error"] = @"SELF public header is truncated";
            [probes addObject:probe];
            [summary appendFormat:@"\n✗ %@ (header truncated)", relativePath];
            continue;
        }

        const auto *headerBytes =
            static_cast<const std::uint8_t *>(headerData.bytes);
        const auto parsedSelf = vshift::loader::ParsePs4SelfHeaders(
            std::span<const std::uint8_t>(headerBytes, headerData.length),
            fileSize);
        if (!parsedSelf.ok()) {
            probe[@"error"] = [NSString stringWithUTF8String:
                parsedSelf.error.c_str()];
            [probes addObject:probe];
            [summary appendFormat:@"\n✗ %@ (%s)", relativePath,
                parsedSelf.error.c_str()];
            continue;
        }

        std::size_t loadCount = 0;
        for (const auto &program : parsedSelf.elf.program_headers) {
            if (program.type == vshift::loader::kElfProgramLoad) {
                ++loadCount;
            }
        }
        const auto mappings = vshift::loader::MatchSelfLoadEntries(parsedSelf);
        probe[@"metadata_size"] = @(parsedSelf.header.metadata_size);
        probe[@"entry_count"] = @(parsedSelf.header.entry_count);
        probe[@"elf_offset"] = @(parsedSelf.elf.offset);
        probe[@"elf_machine"] = @(parsedSelf.elf.header.machine);
        probe[@"elf_entry"] = [NSString stringWithFormat:@"0x%llx",
            static_cast<unsigned long long>(parsedSelf.elf.header.entry)];
        probe[@"elf_program_headers"] = @(parsedSelf.elf.program_headers.size());
        probe[@"elf_load_segments"] = @(loadCount);
        probe[@"payload_map_count"] = @(mappings.size());
        probe[@"payload_state"] =
            mappings.size() == loadCount
                ? @"size-correlated payload map; decryption pending"
                : @"partial payload map; decryption pending";
        [probes addObject:probe];
        ++validCount;

        NSString *machine = parsedSelf.elf.header.machine ==
                                    vshift::loader::kElfMachineX86_64
                                ? @"x86_64"
                                : [NSString stringWithFormat:@"machine %u",
                                    parsedSelf.elf.header.machine];
        [summary appendFormat:
            @"\n✓ %@\n  SELF 0x%08x, ELF %@\n  Entry %@, PT_LOAD %lu, map %lu/%lu",
            relativePath, parsedSelf.header.magic, machine, probe[@"elf_entry"],
            static_cast<unsigned long>(loadCount),
            static_cast<unsigned long>(mappings.size()),
            static_cast<unsigned long>(loadCount)];
    }

    NSDictionary *manifest = @{
        @"schema_version": @4,
        @"source_file_name": url.lastPathComponent ?: @"firmware-root",
        @"source_kind": @"user-provided decrypted PS4 firmware root",
        @"boot_profile": @"PS4 VSH SELF probe",
        @"root_path": url.path ?: @"",
        @"self_probe": probes,
        @"boot_status": validCount > 0
            ? @"PS4 SELF headers validated; payload execution pending"
            : @"No valid PS4 SELF entry was found",
    };

    if (accessed) {
        [url stopAccessingSecurityScopedResource];
    }
    NSError *manifestError = nil;
    const BOOL manifestSaved = [self writeManifest:manifest error:&manifestError];
    [summary appendFormat:@"\n%@\n%@",
        validCount > 0
            ? @"Real PS4 SELF metadata is mapped; execution is the next stage."
            : @"Choose the extracted Firmware 5.05 root.",
        manifestSaved
            ? @"Manifest saved."
            : [NSString stringWithFormat:@"Manifest save failed: %@",
                manifestError.localizedDescription ?: @"unknown error"]];
    self.statusLabel.text = summary;
}

- (void)inspectDecryptedFirmwareAtURL:(NSURL *)url {
    const BOOL accessed = [url startAccessingSecurityScopedResource];
    NSError *attributesError = nil;
    NSDictionary *attributes =
        [[NSFileManager defaultManager] attributesOfItemAtPath:url.path
                                                          error:&attributesError];
    NSNumber *fileSizeNumber = attributes[NSFileSize];
    const auto fileSize = fileSizeNumber != nil
                              ? fileSizeNumber.unsignedLongLongValue
                              : 0;
    if (attributesError != nil ||
        fileSize < vshift::firmware::kPupFragmentPublicHeaderSize) {
        self.statusLabel.text =
            @"Decrypted PUP is too small or unreadable.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    NSError *openError = nil;
    NSFileHandle *handle =
        [NSFileHandle fileHandleForReadingFromURL:url error:&openError];
    if (handle == nil) {
        self.statusLabel.text = [NSString stringWithFormat:
            @"Could not open decrypted PUP: %@",
            openError.localizedDescription ?: @"unknown error"];
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    NSData *fixedHeaderData =
        [handle readDataOfLength:vshift::firmware::kPupFixedHeaderSize];
    if (fixedHeaderData.length < vshift::firmware::kPupFixedHeaderSize) {
        [handle closeFile];
        self.statusLabel.text = @"Could not read decrypted PUP header.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    const auto *fixedHeaderBytes =
        static_cast<const std::uint8_t *>(fixedHeaderData.bytes);
    const auto publicHeader = vshift::firmware::ParsePupFragmentHeader(
        std::span<const std::uint8_t>(
            fixedHeaderBytes, vshift::firmware::kPupFixedHeaderSize));
    if (!publicHeader.ok()) {
        [handle closeFile];
        self.statusLabel.text = [NSString stringWithFormat:
            @"Decrypted PUP header failed:\n%s", publicHeader.error.c_str()];
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    constexpr std::uint16_t kMaximumHeaderSize = 16 * 1024;
    if (publicHeader.header.header_size < vshift::firmware::kPupFixedHeaderSize ||
        publicHeader.header.header_size > kMaximumHeaderSize ||
        publicHeader.header.header_size > fileSize) {
        [handle closeFile];
        self.statusLabel.text = @"Decrypted PUP header size is invalid.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    [handle seekToFileOffset:0];
    NSData *headerData =
        [handle readDataOfLength:publicHeader.header.header_size];
    if (headerData.length < publicHeader.header.header_size) {
        [handle closeFile];
        self.statusLabel.text = @"Could not read the decrypted PUP table.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    const auto *headerBytes =
        static_cast<const std::uint8_t *>(headerData.bytes);
    const auto parsed = vshift::firmware::ParseDecryptedPupHeaders(
        std::span<const std::uint8_t>(headerBytes, headerData.length),
        fileSize);
    if (!parsed.ok()) {
        [handle closeFile];
        self.statusLabel.text = [NSString stringWithFormat:
            @"Decrypted PUP table failed:\n%s", parsed.error.c_str()];
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    NSMutableArray *segments =
        [NSMutableArray arrayWithCapacity:parsed.segments.size()];
    for (std::size_t index = 0; index < parsed.segments.size(); ++index) {
        const auto &segment = parsed.segments[index];
        [segments addObject:@{
            @"index": @(index),
            @"flags": @(segment.flags),
            @"offset": @(segment.offset),
            @"compressed_size": @(segment.compressed_size),
            @"uncompressed_size": @(segment.uncompressed_size),
        }];
    }

    // Scan only bounded prefixes of PUP segments. This identifies the PS5
    // SELF container and its embedded ELF table without touching payload code.
    constexpr std::size_t kSelfPrefixLimit = 16 * 1024;
    NSMutableArray *selfCandidates = [NSMutableArray array];
    std::size_t selfMagicCount = 0;
    std::size_t validSelfCount = 0;
    NSDictionary *primarySelfCandidate = nil;
    for (std::size_t index = 0; index < parsed.segments.size(); ++index) {
        const auto &segment = parsed.segments[index];
        if (segment.compressed_size < vshift::loader::kSelfHeaderSize ||
            segment.offset > fileSize ||
            segment.compressed_size > fileSize - segment.offset) {
            continue;
        }

        [handle seekToFileOffset:segment.offset];
        const auto fixedLength = static_cast<NSUInteger>(std::min<std::uint64_t>(
            segment.compressed_size, vshift::loader::kSelfHeaderSize));
        NSData *selfFixedData = [handle readDataOfLength:fixedLength];
        if (selfFixedData.length < sizeof(std::uint32_t)) {
            continue;
        }
        const auto *selfFixedBytes =
            static_cast<const std::uint8_t *>(selfFixedData.bytes);
        if (ReadU32LE(selfFixedBytes) != vshift::loader::kPs5SelfMagic) {
            continue;
        }
        ++selfMagicCount;

        NSMutableDictionary *candidate = [@{
            @"pup_segment_index": @(index),
            @"offset": @(segment.offset),
            @"compressed_size": @(segment.compressed_size),
            @"uncompressed_size": @(segment.uncompressed_size),
            @"magic": @"0xeef51454",
        } mutableCopy];

        if (selfFixedData.length < vshift::loader::kSelfHeaderSize) {
            candidate[@"parse_error"] = @"truncated SELF header";
            [selfCandidates addObject:candidate];
            continue;
        }

        const auto selfHeaderSize = ReadU16LE(selfFixedBytes + 0x0C);
        if (selfHeaderSize < vshift::loader::kSelfHeaderSize ||
            selfHeaderSize > kSelfPrefixLimit ||
            selfHeaderSize > segment.compressed_size) {
            candidate[@"parse_error"] = @"invalid SELF header size";
            [selfCandidates addObject:candidate];
            continue;
        }

        [handle seekToFileOffset:segment.offset];
        NSData *selfHeaderData =
            [handle readDataOfLength:static_cast<NSUInteger>(selfHeaderSize)];
        const auto *selfHeaderBytes =
            static_cast<const std::uint8_t *>(selfHeaderData.bytes);
        const auto self = vshift::loader::ParsePs5SelfHeaders(
            std::span<const std::uint8_t>(selfHeaderBytes,
                                          selfHeaderData.length),
            segment.compressed_size);
        if (!self.ok()) {
            candidate[@"parse_error"] = [NSString stringWithUTF8String:
                self.error.c_str()];
            [selfCandidates addObject:candidate];
            continue;
        }

        std::size_t loadSegmentCount = 0;
        for (const auto &programHeader : self.elf.program_headers) {
            if (programHeader.type == vshift::loader::kElfProgramLoad) {
                ++loadSegmentCount;
            }
        }
        const auto mappings = vshift::loader::MatchSelfLoadEntries(self);
        NSMutableArray *loadMappings = [NSMutableArray arrayWithCapacity:
            mappings.size()];
        for (const auto &mapping : mappings) {
            [loadMappings addObject:@{
                @"elf_program_header_index": @(mapping.program_header_index),
                @"self_entry_index": @(mapping.self_entry_index),
                @"physical_offset": @(mapping.physical_offset),
                @"virtual_address": [NSString stringWithFormat:
                    @"0x%llx",
                    static_cast<unsigned long long>(mapping.virtual_address)],
                @"file_size": @(mapping.file_size),
                @"memory_size": @(mapping.memory_size),
            }];
        }
        candidate[@"header_size"] = @(self.header.header_size);
        candidate[@"metadata_size"] = @(self.header.metadata_size);
        candidate[@"entry_count"] = @(self.header.entry_count);
        candidate[@"elf_offset"] = @(self.elf.offset);
        candidate[@"elf_type"] = @(self.elf.header.type);
        candidate[@"elf_machine"] = @(self.elf.header.machine);
        candidate[@"elf_entry"] = [NSString stringWithFormat:
            @"0x%llx", static_cast<unsigned long long>(self.elf.header.entry)];
        candidate[@"elf_program_headers"] = @(self.elf.program_headers.size());
        candidate[@"elf_load_segments"] = @(loadSegmentCount);
        candidate[@"load_mappings"] = loadMappings;
        candidate[@"payload_state"] =
            mappings.size() == loadSegmentCount
                ? @"size-correlated payload map; decryption pending"
                : @"partial payload map; decryption pending";
        [selfCandidates addObject:candidate];
        ++validSelfCount;
        if (primarySelfCandidate == nil) {
            primarySelfCandidate = candidate;
        }
    }

    [handle closeFile];

    NSString *bootStatus = validSelfCount > 0
        ? @"PUP + SELF/ELF validated; size-correlated payload map ready"
        : (selfMagicCount > 0
            ? @"PUP table valid; SELF candidate needs more header data"
            : @"PUP table validated; SELF/ELF candidate not found");

    NSDictionary *manifest = @{
        @"schema_version": @2,
        @"source_file_name": url.lastPathComponent ?: @"PS5UPDATE1.PUP.dec",
        @"source_kind": @"user-provided decrypted component",
        @"container_format": @"PS5 PUP/SELF public header",
        @"container_size": @(fileSize),
        @"decrypted": @YES,
        @"public_header": @{
            @"magic": [NSString stringWithFormat:@"0x%08x",
                        parsed.header.public_header.magic],
            @"version": @(parsed.header.public_header.version),
            @"mode": @(parsed.header.public_header.mode),
            @"endian": @(parsed.header.public_header.endian),
            @"attributes": @(parsed.header.public_header.attributes),
            @"key_type": @(parsed.header.public_header.key_type),
            @"header_size": @(parsed.header.public_header.header_size),
            @"metadata_size": @(parsed.header.public_header.metadata_size),
        },
        @"pup": @{
            @"file_size": @(parsed.header.file_size),
            @"segment_count": @(parsed.header.segment_count),
            @"flags": @(parsed.header.flags),
            @"firmware_version": [NSString stringWithFormat:@"0x%08x",
                                  parsed.header.firmware_version],
            @"segments": segments,
        },
        @"self_candidates": selfCandidates,
        @"boot_status": bootStatus,
    };

    if (accessed) {
        [url stopAccessingSecurityScopedResource];
    }

    NSError *manifestError = nil;
    const BOOL manifestSaved = [self writeManifest:manifest error:&manifestError];
    NSString *segmentSummary = [NSString stringWithFormat:
        @"Segments: %u", parsed.header.segment_count];
    if (!parsed.segments.empty()) {
        const auto &first = parsed.segments.front();
        segmentSummary = [NSString stringWithFormat:
            @"Segments: %u\nFirst payload: 0x%llx (%llu bytes)",
            parsed.header.segment_count,
            static_cast<unsigned long long>(first.offset),
            static_cast<unsigned long long>(first.compressed_size)];
    }
    NSMutableString *summary = [NSMutableString stringWithFormat:
        @"DECRYPTED PUP READY\n%@\nSize: %llu bytes\nFirmware: 0x%08x\n%@",
        url.lastPathComponent ?: @"PS5UPDATE1.PUP.dec",
        static_cast<unsigned long long>(fileSize),
        parsed.header.firmware_version,
        segmentSummary];
    if (validSelfCount > 0) {
        NSDictionary *candidate = primarySelfCandidate;
        [summary appendFormat:
            @"\nSELF: segment %@\nELF: %@, entry %@\nPT_LOAD: %@",
            candidate[@"pup_segment_index"],
            [candidate[@"elf_machine"] integerValue] ==
                    vshift::loader::kElfMachineX86_64
                ? @"x86_64"
                : [NSString stringWithFormat:@"machine %lld",
                    [candidate[@"elf_machine"] longLongValue]],
            candidate[@"elf_entry"],
            candidate[@"elf_load_segments"]];
        NSArray *mappings = candidate[@"load_mappings"];
        [summary appendFormat:@"\nPayload map: %@/%@ PT_LOADs",
                              @(mappings.count), candidate[@"elf_load_segments"]];
    } else {
        [summary appendFormat:@"\nSELF magic candidates: %lu",
                              static_cast<unsigned long>(selfMagicCount)];
    }
    [summary appendFormat:@"\n%@\n%@",
        bootStatus,
        manifestSaved
            ? @"Manifest saved. SELF/ELF header map is ready."
            : [NSString stringWithFormat:@"Manifest save failed: %@",
                manifestError.localizedDescription ?: @"unknown error"]];
    self.statusLabel.text = summary;
}

- (BOOL)writeManifest:(NSDictionary *)manifest error:(NSError **)error {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *applicationSupport =
        [fileManager URLForDirectory:NSApplicationSupportDirectory
                            inDomain:NSUserDomainMask
                   appropriateForURL:nil
                              create:YES
                               error:error];
    if (applicationSupport == nil) {
        return NO;
    }

    NSURL *vshiftDirectory =
        [applicationSupport URLByAppendingPathComponent:@"VSHift"
                                             isDirectory:YES];
    if (![fileManager createDirectoryAtURL:vshiftDirectory
                withIntermediateDirectories:YES
                                 attributes:nil
                                      error:error]) {
        return NO;
    }

    NSData *manifestData =
        [NSJSONSerialization dataWithJSONObject:manifest
                                         options:NSJSONWritingPrettyPrinted
                                           error:error];
    if (manifestData == nil) {
        return NO;
    }

    NSURL *manifestURL =
        [vshiftDirectory URLByAppendingPathComponent:@"firmware-manifest.json"];
    const BOOL saved = [manifestData writeToURL:manifestURL
                                        options:NSDataWritingAtomic
                                          error:error];
    if (saved) {
        self.manifestURL = manifestURL;
        self.exportManifestButton.enabled = YES;
    }
    return saved;
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller
 didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    (void)controller;
    NSURL *url = urls.firstObject;
    if (url == nil) {
        return;
    }

    if (self.pickingDecryptedFirmware) {
        self.pickingDecryptedFirmware = NO;
        [self inspectDecryptedFirmwareAtURL:url];
        return;
    }

    if (self.pickingFirmwareRoot) {
        self.pickingFirmwareRoot = NO;
        [self inspectFirmwareRootAtURL:url];
        return;
    }

    if (self.pickingFirmwareSelfProbe) {
        self.pickingFirmwareSelfProbe = NO;
        [self probeFirmwareSelfAtURL:url];
        return;
    }

    const BOOL accessed = [url startAccessingSecurityScopedResource];
    NSError *attributesError = nil;
    NSDictionary *attributes =
        [[NSFileManager defaultManager] attributesOfItemAtPath:url.path
                                                          error:&attributesError];
    NSNumber *fileSizeNumber = attributes[NSFileSize];
    const auto fileSize = fileSizeNumber != nil
                              ? fileSizeNumber.unsignedLongLongValue
                              : 0;
    if (attributesError != nil || fileSize < vshift::firmware::kSlb2HeaderSize) {
        self.statusLabel.text = @"Firmware file is too small or unreadable.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    NSFileHandle *handle = [NSFileHandle fileHandleForReadingFromURL:url
                                                                 error:nil];
    if (handle == nil) {
        self.statusLabel.text = @"Could not open firmware file.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }
    NSData *headerData = [handle readDataOfLength:vshift::firmware::kSlb2HeaderSize];
    if (headerData.length < vshift::firmware::kSlb2HeaderSize) {
        self.statusLabel.text = @"Could not read firmware header.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    const auto *header = static_cast<const std::uint8_t *>(headerData.bytes);
    if (header[0] != 'S' || header[1] != 'L' || header[2] != 'B' ||
        header[3] != '2') {
        self.statusLabel.text = @"This is not a PS5 SLB2 firmware container.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    const auto entryCount = ReadU32LE(header + 0x0C);
    constexpr std::uint32_t kMaximumEntries = 1'000'000;
    if (entryCount > kMaximumEntries) {
        self.statusLabel.text = @"Firmware table entry count is invalid.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    const auto tableSize = vshift::firmware::kSlb2HeaderSize +
                           static_cast<std::size_t>(entryCount) *
                               vshift::firmware::kSlb2EntrySize;
    if (tableSize > fileSize ||
        tableSize > static_cast<std::size_t>(
                         std::numeric_limits<NSInteger>::max())) {
        self.statusLabel.text = @"Firmware file table is invalid.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    [handle seekToFileOffset:0];
    NSData *tableData = [handle readDataOfLength:tableSize];
    if (tableData.length != tableSize) {
        self.statusLabel.text = @"Could not read firmware file table.";
        [handle closeFile];
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    const auto *tableBytes = static_cast<const std::uint8_t *>(tableData.bytes);
    const auto parsed = vshift::firmware::ParseSlb2Table(
        std::span<const std::uint8_t>(tableBytes, tableData.length), fileSize);
    if (!parsed.ok()) {
    self.statusLabel.text = [NSString stringWithFormat:
            @"Firmware parse failed:\n%s", parsed.error.c_str()];
        [handle closeFile];
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    const vshift::firmware::ReadOnlyFirmwareCatalog catalog(parsed.package);
    std::size_t fragmentIndex = parsed.package.entries.size();
    for (std::size_t index = 0; index < parsed.package.entries.size(); ++index) {
        if (parsed.package.entries[index].size >=
            vshift::firmware::kPupFragmentPublicHeaderSize) {
            fragmentIndex = index;
            break;
        }
    }

    vshift::firmware::PupFragmentParseResult fragment;
    bool fragmentRead = false;
    if (fragmentIndex < parsed.package.entries.size()) {
        const auto &entry = parsed.package.entries[fragmentIndex];
        const auto range = catalog.Resolve(
            entry.name, 0, vshift::firmware::kPupFragmentPublicHeaderSize);
        if (range.ok()) {
            [handle seekToFileOffset:range.range.absolute_offset];
            NSData *fragmentData =
                [handle readDataOfLength:
                             vshift::firmware::kPupFragmentPublicHeaderSize];
            if (fragmentData.length ==
                vshift::firmware::kPupFragmentPublicHeaderSize) {
                const auto *fragmentBytes =
                    static_cast<const std::uint8_t *>(fragmentData.bytes);
                fragment = vshift::firmware::ParsePupFragmentHeader(
                    std::span<const std::uint8_t>(
                        fragmentBytes, fragmentData.length));
                fragmentRead = true;
            }
        }
    }

    NSMutableArray *components = [NSMutableArray arrayWithCapacity:
        parsed.package.entries.size()];
    for (std::size_t index = 0; index < parsed.package.entries.size(); ++index) {
        const auto &entry = parsed.package.entries[index];
        NSString *name = [NSString stringWithUTF8String:entry.name.c_str()];
        NSMutableDictionary *component = [@{
            @"name": name ?: @"(unnamed)",
            @"offset": @(entry.offset),
            @"size": @(entry.size),
        } mutableCopy];
        if (index == fragmentIndex && fragmentRead && fragment.ok()) {
            component[@"fragment_header"] = @{
                @"format": @"PS5 PUP/SELF public header",
                @"magic": [NSString stringWithFormat:@"0x%08x",
                            fragment.header.magic],
                @"version": @(fragment.header.version),
                @"mode": @(fragment.header.mode),
                @"endian": @(fragment.header.endian),
                @"attributes": @(fragment.header.attributes),
                @"key_type": @(fragment.header.key_type),
                @"header_size": @(fragment.header.header_size),
                @"metadata_size": @(fragment.header.metadata_size),
            };
        }
        [components addObject:component];
    }

    NSDictionary *manifest = @{
        @"schema_version": @1,
        @"source_file_name": url.lastPathComponent ?: @"PS5UPDATE.PUP",
        @"container_format": @"SLB2",
        @"container_size": @(fileSize),
        @"slb2": @{
            @"version": @(parsed.package.version),
            @"flags": @(parsed.package.flags),
            @"entry_count": @(parsed.package.entry_count),
            @"size_in_sectors": @(parsed.package.size_in_sectors),
        },
        @"components": components,
    };

    [handle closeFile];
    if (accessed) {
        [url stopAccessingSecurityScopedResource];
    }

    NSError *manifestError = nil;
    const BOOL manifestSaved = [self writeManifest:manifest
                                             error:&manifestError];

    NSMutableString *summary = [NSMutableString stringWithFormat:
        @"SLB2 OK\n%d components", parsed.package.entry_count];
    const std::size_t visibleEntries =
        std::min<std::size_t>(parsed.package.entries.size(), 8);
    for (std::size_t index = 0; index < visibleEntries; ++index) {
        const auto &entry = parsed.package.entries[index];
        [summary appendFormat:@"\n• %s (%llu bytes)", entry.name.c_str(),
                              static_cast<unsigned long long>(entry.size)];
    }
    if (parsed.package.entries.size() > visibleEntries) {
        [summary appendFormat:@"\n… and %d more",
                              static_cast<int>(parsed.package.entries.size() -
                                               visibleEntries)];
    }
    if (fragmentRead && fragment.ok()) {
        [summary appendFormat:@"\nPS5 fragment: 0x%08x, header 0x%x",
                              fragment.header.magic,
                              fragment.header.header_size];
    } else if (fragmentRead) {
        [summary appendFormat:@"\nFragment header: %s",
                              fragment.error.c_str()];
    }
    if (manifestSaved) {
        [summary appendString:@"\nManifest saved"];
    } else {
        [summary appendFormat:@"\nManifest save failed: %@",
                              manifestError.localizedDescription ?: @"unknown error"];
    }
    self.statusLabel.text = summary;
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
