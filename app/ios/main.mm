#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "core/boot/synthetic_boot.h"
#include "core/boot/ps4_boot.h"
#include "core/cpu/arm64_jit.h"
#include "core/firmware/catalog.h"
#include "core/firmware/pup.h"
#include "core/firmware/ps3_package.h"
#include "core/firmware/ps3_pup.h"
#include "core/firmware/ps3_tar.h"
#include "core/firmware/slb2.h"
#include "core/cpu/x86_decoder.h"
#include "core/loader/elf_loader.h"
#include "core/loader/self.h"
#include "core/loader/self_loader.h"
#include "core/loader/ps3_sce.h"
#include "core/memory/guest_memory.h"
#include "core/video/framebuffer.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

@interface VSHiftJITViewController : UIViewController <UIDocumentPickerDelegate>
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UILabel *guestFrameStateLabel;
@property(nonatomic, strong) UIImageView *guestFrameView;
@property(nonatomic, strong) UIButton *exportManifestButton;
@property(nonatomic, strong) UIButton *ps3BootButton;
@property(nonatomic, strong) NSURL *manifestURL;
@property(nonatomic, strong) NSURL *firmwareRootURL;
@property(nonatomic, strong) NSURL *ps3FirmwareURL;
@property(nonatomic, assign) BOOL pickingDecryptedFirmware;
@property(nonatomic, assign) BOOL pickingPs3Firmware;
@property(nonatomic, assign) BOOL pickingFirmwareRoot;
@property(nonatomic, assign) BOOL pickingFirmwareSelfProbe;
@property(nonatomic, assign) BOOL pickingRealBoot;
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

static std::uint64_t ReadU64BE(const std::uint8_t *bytes) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = (value << 8) | bytes[index];
    }
    return value;
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

    UIView *displayCard = [[UIView alloc] init];
    displayCard.backgroundColor = UIColor.blackColor;
    displayCard.layer.cornerRadius = 24.0;
    displayCard.clipsToBounds = YES;
    displayCard.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);

    self.guestFrameView = [[UIImageView alloc] init];
    self.guestFrameView.translatesAutoresizingMaskIntoConstraints = NO;
    self.guestFrameView.contentMode = UIViewContentModeScaleAspectFit;
    self.guestFrameView.hidden = YES;
    self.guestFrameView.accessibilityLabel = @"Guest framebuffer";

    self.guestFrameStateLabel = [[UILabel alloc] init];
    self.guestFrameStateLabel.text = @"Waiting for guest video output";
    self.guestFrameStateLabel.textColor = UIColor.whiteColor;
    self.guestFrameStateLabel.textAlignment = NSTextAlignmentCenter;
    self.guestFrameStateLabel.numberOfLines = 0;
    self.guestFrameStateLabel.translatesAutoresizingMaskIntoConstraints = NO;

    [displayCard addSubview:self.guestFrameView];
    [displayCard addSubview:self.guestFrameStateLabel];

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

    UIButtonConfiguration *ps3Configuration =
        [UIButtonConfiguration tintedButtonConfiguration];
    ps3Configuration.title = @"Import PS3UPDAT.PUP";
    ps3Configuration.image = [UIImage systemImageNamed:@"gamecontroller"];
    ps3Configuration.imagePadding = 8.0;
    ps3Configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    UIButton *ps3Button = [UIButton buttonWithType:UIButtonTypeSystem];
    ps3Button.configuration = ps3Configuration;
    [ps3Button addTarget:self
                  action:@selector(importPs3Firmware)
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

    UIButtonConfiguration *ps3BootConfiguration =
        [UIButtonConfiguration filledButtonConfiguration];
    ps3BootConfiguration.title = @"Prepare PS3 VSH bootstrap";
    ps3BootConfiguration.image = [UIImage systemImageNamed:@"play.tv"];
    ps3BootConfiguration.imagePadding = 8.0;
    ps3BootConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    self.ps3BootButton = [UIButton buttonWithType:UIButtonTypeSystem];
    self.ps3BootButton.configuration = ps3BootConfiguration;
    self.ps3BootButton.enabled = NO;
    [self.ps3BootButton addTarget:self
                           action:@selector(preparePs3VshBootstrap)
                 forControlEvents:UIControlEventTouchUpInside];

    UIView *firmwareCard = [[UIView alloc] init];
    firmwareCard.backgroundColor = UIColor.secondarySystemBackgroundColor;
    firmwareCard.layer.cornerRadius = 24.0;
    firmwareCard.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);
    UIStackView *firmwareStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        firmwareHeader, ps3Button, rootButton,
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

    UIButtonConfiguration *realBootConfiguration =
        [UIButtonConfiguration filledButtonConfiguration];
    realBootConfiguration.title = @"Attempt real PS4 boot";
    realBootConfiguration.image = [UIImage systemImageNamed:@"play.tv"];
    realBootConfiguration.imagePadding = 8.0;
    realBootConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    UIButton *realBootButton = [UIButton buttonWithType:UIButtonTypeSystem];
    realBootButton.configuration = realBootConfiguration;
    [realBootButton addTarget:self
                       action:@selector(attemptRealPS4Boot)
             forControlEvents:UIControlEventTouchUpInside];

    UIView *bootCard = [[UIView alloc] init];
    bootCard.backgroundColor = UIColor.secondarySystemBackgroundColor;
    bootCard.layer.cornerRadius = 24.0;
    bootCard.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);
    UIStackView *bootStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        bootHeader, self.ps3BootButton, realBootButton, selfProbeButton,
        syntheticJitButton, syntheticJitLessButton
    ]];
    bootStack.axis = UILayoutConstraintAxisVertical;
    bootStack.spacing = 12.0;
    bootStack.translatesAutoresizingMaskIntoConstraints = NO;
    [bootCard addSubview:bootStack];

    [content addArrangedSubview:header];
    [content addArrangedSubview:statusCard];
    [content addArrangedSubview:displayCard];
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
        [displayCard.heightAnchor constraintEqualToConstant:220.0],
        [self.guestFrameView.leadingAnchor constraintEqualToAnchor:displayCard.layoutMarginsGuide.leadingAnchor],
        [self.guestFrameView.trailingAnchor constraintEqualToAnchor:displayCard.layoutMarginsGuide.trailingAnchor],
        [self.guestFrameView.topAnchor constraintEqualToAnchor:displayCard.layoutMarginsGuide.topAnchor],
        [self.guestFrameView.bottomAnchor constraintEqualToAnchor:displayCard.layoutMarginsGuide.bottomAnchor],
        [self.guestFrameStateLabel.leadingAnchor constraintEqualToAnchor:displayCard.layoutMarginsGuide.leadingAnchor],
        [self.guestFrameStateLabel.trailingAnchor constraintEqualToAnchor:displayCard.layoutMarginsGuide.trailingAnchor],
        [self.guestFrameStateLabel.centerYAnchor constraintEqualToAnchor:displayCard.centerYAnchor],
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
    self.pickingPs3Firmware = NO;
    self.pickingFirmwareRoot = NO;
    self.pickingFirmwareSelfProbe = NO;
    self.pickingRealBoot = NO;
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
    self.pickingPs3Firmware = NO;
    self.pickingFirmwareRoot = NO;
    self.pickingFirmwareSelfProbe = NO;
    self.pickingRealBoot = NO;
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeData]
                                 asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)importPs3Firmware {
    self.pickingDecryptedFirmware = NO;
    self.pickingPs3Firmware = YES;
    self.pickingFirmwareRoot = NO;
    self.pickingFirmwareSelfProbe = NO;
    self.pickingRealBoot = NO;
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
    self.pickingPs3Firmware = NO;
    self.pickingFirmwareRoot = YES;
    self.pickingFirmwareSelfProbe = NO;
    self.pickingRealBoot = NO;
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
    self.pickingPs3Firmware = NO;
    self.pickingFirmwareRoot = NO;
    self.pickingFirmwareSelfProbe = YES;
    self.pickingRealBoot = NO;
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeFolder]
                                 asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)attemptRealPS4Boot {
    if (self.firmwareRootURL == nil) {
        self.statusLabel.text =
            @"Choose Firmware 5.05 root first. The real boot attempt will then read SceSysCore.elf.";
        self.pickingDecryptedFirmware = NO;
        self.pickingPs3Firmware = NO;
        self.pickingFirmwareRoot = NO;
        self.pickingFirmwareSelfProbe = NO;
        self.pickingRealBoot = YES;
        UIDocumentPickerViewController *picker =
            [[UIDocumentPickerViewController alloc]
                initForOpeningContentTypes:@[UTTypeFolder]
                                     asCopy:NO];
        picker.delegate = self;
        picker.allowsMultipleSelection = NO;
        [self presentViewController:picker animated:YES completion:nil];
        return;
    }

    [self attemptRealPS4BootAtURL:self.firmwareRootURL];
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

- (void)attemptRealPS4BootAtURL:(NSURL *)rootURL {
    const BOOL accessed = [rootURL startAccessingSecurityScopedResource];
    self.guestFrameView.image = nil;
    self.guestFrameView.hidden = YES;
    self.guestFrameStateLabel.text = @"Waiting for guest video output";
    vshift::boot::Ps4BootSession session(
        [self](const vshift::video::GuestFrame& frame) {
            return [self presentGuestFrame:frame];
        });
    const auto report = session.Run(
        [&](std::string_view relativePath) -> vshift::boot::BootFile {
            NSString *relative = [NSString stringWithUTF8String:
                std::string(relativePath).c_str()];
            NSURL *moduleURL = [rootURL URLByAppendingPathComponent:relative];
            NSDictionary *attributes = [[NSFileManager defaultManager]
                attributesOfItemAtPath:moduleURL.path error:nil];
            NSNumber *sizeNumber = attributes[NSFileSize];
            const auto fileSize = sizeNumber != nil
                                      ? sizeNumber.unsignedLongLongValue
                                      : 0;
            if (fileSize == 0 || fileSize > 512ull * 1024ull * 1024ull) {
                return {{}, "module is missing or too large"};
            }
            NSFileHandle *handle =
                [NSFileHandle fileHandleForReadingFromURL:moduleURL error:nil];
            if (handle == nil) {
                return {{}, "module could not be opened"};
            }
            NSData *data = [handle readDataToEndOfFile];
            [handle closeFile];
            if (data.length != fileSize) {
                return {{}, "module could not be read completely"};
            }
            const auto *bytes =
                static_cast<const std::uint8_t *>(data.bytes);
            return {std::vector<std::uint8_t>(bytes, bytes + data.length), {}};
        });
    if (accessed) {
        [rootURL stopAccessingSecurityScopedResource];
    }
    if (!report.ok()) {
        NSString *stage = @"firmware root";
        switch (report.stage) {
        case vshift::boot::Ps4BootStage::SysCore:
            stage = @"SceSysCore";
            break;
        case vshift::boot::Ps4BootStage::ShellCore:
            stage = @"SceShellCore";
            break;
        case vshift::boot::Ps4BootStage::GuestExecution:
            stage = @"guest CPU";
            break;
        default:
            break;
        }
        self.statusLabel.text = [NSString stringWithFormat:
            @"REAL PS4 BOOT STOPPED\nStage: %@\n%s", stage,
            report.error.c_str()];
        return;
    }
    const auto *frame = session.video_output().last_frame();
    if (frame != nullptr) {
        self.statusLabel.text = [NSString stringWithFormat:
            @"PS4 GUEST FRAME PRESENTED\nSceSysCore: %lu segments\nSceShellCore: %lu segments\nFrame: %ux%u",
            static_cast<unsigned long>(report.syscore.mapped_segments),
            static_cast<unsigned long>(report.shellcore.mapped_segments),
            frame->description.width, frame->description.height];
    } else if (report.modules_mapped()) {
        self.statusLabel.text = [NSString stringWithFormat:
            @"REAL PS4 BOOT PATH READY\nSceSysCore: %lu segments\nSceShellCore: %lu segments\nNo guest video frame was submitted.",
            static_cast<unsigned long>(report.syscore.mapped_segments),
            static_cast<unsigned long>(report.shellcore.mapped_segments)];
    } else {
        self.statusLabel.text = @"REAL PS4 BOOT STOPPED\nNo modules were mapped.";
    }
}

- (BOOL)presentGuestFrame:(const vshift::video::GuestFrame&)frame {
    if (frame.description.format != vshift::video::PixelFormat::Rgba8 &&
        frame.description.format != vshift::video::PixelFormat::Bgra8) {
        self.guestFrameStateLabel.text = @"Unsupported guest pixel format";
        return NO;
    }

    NSData *pixelData = [NSData dataWithBytes:frame.pixels.data()
                                       length:frame.pixels.size()];
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(
        (__bridge CFDataRef)pixelData);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bitmapInfo = frame.description.format ==
                                      vshift::video::PixelFormat::Bgra8
                                  ? (kCGBitmapByteOrder32Little |
                                     kCGImageAlphaFirst)
                                  : (kCGBitmapByteOrderDefault |
                                     kCGImageAlphaLast);
    CGImageRef image = CGImageCreate(
        frame.description.width, frame.description.height, 8, 32,
        frame.description.bytes_per_row, colorSpace, bitmapInfo, provider,
        nullptr, false, kCGRenderingIntentDefault);
    if (image == nullptr) {
        CGColorSpaceRelease(colorSpace);
        CGDataProviderRelease(provider);
        self.guestFrameStateLabel.text = @"Guest frame could not be decoded";
        return NO;
    }

    self.guestFrameView.image = [UIImage imageWithCGImage:image];
    self.guestFrameView.hidden = NO;
    self.guestFrameStateLabel.text = [NSString stringWithFormat:
        @"Guest framebuffer · %ux%u",
        frame.description.width, frame.description.height];
    CGImageRelease(image);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
    return YES;
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
            ? @"PS4 firmware root preflight passed; next stage is SELF payload availability"
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
        std::size_t encryptedCount = 0;
        std::size_t compressedCount = 0;
        std::size_t blockedCount = 0;
        for (const auto &entry : parsedSelf.entries) {
            encryptedCount += entry.is_encrypted() ? 1 : 0;
            compressedCount += entry.is_compressed() ? 1 : 0;
            blockedCount += entry.is_blocked() ? 1 : 0;
        }
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
        probe[@"encrypted_entries"] = @(encryptedCount);
        probe[@"compressed_entries"] = @(compressedCount);
        probe[@"blocked_entries"] = @(blockedCount);
        probe[@"payload_state"] = encryptedCount > 0
            ? @"protected SELF payload; guest code unavailable"
            : (compressedCount > 0
                ? @"compressed SELF payload; decompression pending"
                : (mappings.size() == loadCount
                    ? @"payload map is executable-source ready"
                    : @"partial payload map; source bytes pending"));
        [probes addObject:probe];
        ++validCount;

        NSString *machine = parsedSelf.elf.header.machine ==
                                    vshift::loader::kElfMachineX86_64
                                ? @"x86_64"
                                : [NSString stringWithFormat:@"machine %u",
                                    parsedSelf.elf.header.machine];
        [summary appendFormat:
            @"\n✓ %@\n  SELF 0x%08x, ELF %@\n  Entry %@, PT_LOAD %lu, map %lu/%lu\n  Payload: %lu encrypted, %lu compressed",
            relativePath, parsedSelf.header.magic, machine, probe[@"elf_entry"],
            static_cast<unsigned long>(loadCount),
            static_cast<unsigned long>(mappings.size()),
            static_cast<unsigned long>(loadCount),
            static_cast<unsigned long>(encryptedCount),
            static_cast<unsigned long>(compressedCount)];
    }

    NSDictionary *manifest = @{
        @"schema_version": @4,
        @"source_file_name": url.lastPathComponent ?: @"firmware-root",
        @"source_kind": @"user-provided decrypted PS4 firmware root",
        @"boot_profile": @"PS4 VSH SELF probe",
        @"root_path": url.path ?: @"",
        @"self_probe": probes,
        @"boot_status": validCount > 0
            ? @"PS4 SELF headers validated; protected payload availability reported"
            : @"No valid PS4 SELF entry was found",
    };

    if (accessed) {
        [url stopAccessingSecurityScopedResource];
    }
    NSError *manifestError = nil;
    const BOOL manifestSaved = [self writeManifest:manifest error:&manifestError];
    [summary appendFormat:@"\n%@\n%@",
        validCount > 0
            ? @"Real PS4 SELF metadata is mapped; payload protection is the next blocker."
            : @"Choose the extracted Firmware 5.05 root.",
        manifestSaved
            ? @"Manifest saved."
            : [NSString stringWithFormat:@"Manifest save failed: %@",
                manifestError.localizedDescription ?: @"unknown error"]];
    self.statusLabel.text = summary;
}

- (void)preparePs3VshBootstrap {
    NSURL *url = self.ps3FirmwareURL;
    if (url == nil) {
        self.statusLabel.text =
            @"Import PS3UPDAT.PUP first. The VSH bootstrap is not ready.";
        return;
    }

    const BOOL accessed = [url startAccessingSecurityScopedResource];
    NSDictionary *attributes = [[NSFileManager defaultManager]
        attributesOfItemAtPath:url.path error:nil];
    const auto fileSize = [attributes[NSFileSize] unsignedLongLongValue];
    NSFileHandle *handle =
        [NSFileHandle fileHandleForReadingFromURL:url error:nil];
    if (handle == nil || fileSize < vshift::firmware::kPs3PupHeaderSize) {
        if (handle != nil) [handle closeFile];
        if (accessed) [url stopAccessingSecurityScopedResource];
        self.statusLabel.text = @"Could not open PS3UPDAT.PUP for bootstrap.";
        return;
    }

    NSData *fixedHeaderData =
        [handle readDataOfLength:vshift::firmware::kPs3PupHeaderSize];
    if (fixedHeaderData.length < vshift::firmware::kPs3PupHeaderSize) {
        [handle closeFile];
        if (accessed) [url stopAccessingSecurityScopedResource];
        self.statusLabel.text = @"Could not read the PS3 PUP header.";
        return;
    }
    const auto *fixedHeaderBytes =
        static_cast<const std::uint8_t *>(fixedHeaderData.bytes);
    const auto headerLength = ReadU64BE(fixedHeaderBytes + 0x20);
    if (headerLength < vshift::firmware::kPs3PupHeaderSize ||
        headerLength > 16ull * 1024ull * 1024ull ||
        headerLength > fileSize ||
        headerLength > std::numeric_limits<NSUInteger>::max()) {
        [handle closeFile];
        if (accessed) [url stopAccessingSecurityScopedResource];
        self.statusLabel.text = @"PS3 PUP header size is invalid.";
        return;
    }

    [handle seekToFileOffset:0];
    NSData *headerData =
        [handle readDataOfLength:static_cast<NSUInteger>(headerLength)];
    const auto *headerBytes =
        static_cast<const std::uint8_t *>(headerData.bytes);
    const auto parsed = vshift::firmware::ParsePs3PupHeaders(
        std::span<const std::uint8_t>(headerBytes, headerData.length),
        fileSize);
    if (!parsed.ok()) {
        [handle closeFile];
        if (accessed) [url stopAccessingSecurityScopedResource];
        self.statusLabel.text = [NSString stringWithFormat:
            @"PS3 PUP parse failed:\n%s", parsed.error.c_str()];
        return;
    }

    const auto updateEntry = std::find_if(
        parsed.entries.begin(), parsed.entries.end(), [](const auto& entry) {
            return entry.entry_id == 0x300;
        });
    if (updateEntry == parsed.entries.end() ||
        updateEntry->data_length > std::numeric_limits<NSUInteger>::max()) {
        [handle closeFile];
        if (accessed) [url stopAccessingSecurityScopedResource];
        self.statusLabel.text = @"PS3 PUP has no readable update TAR.";
        return;
    }

    [handle seekToFileOffset:updateEntry->data_offset];
    NSData *updateTarData = [handle readDataOfLength:
        static_cast<NSUInteger>(updateEntry->data_length)];
    if (updateTarData.length != static_cast<NSUInteger>(updateEntry->data_length)) {
        [handle closeFile];
        if (accessed) [url stopAccessingSecurityScopedResource];
        self.statusLabel.text = @"Could not read the PS3 update TAR.";
        return;
    }
    const auto *updateTarBytes =
        static_cast<const std::uint8_t *>(updateTarData.bytes);
    const auto updateTar = vshift::firmware::ParsePs3Tar(
        std::span<const std::uint8_t>(updateTarBytes, updateTarData.length));
    if (!updateTar.ok()) {
        [handle closeFile];
        if (accessed) [url stopAccessingSecurityScopedResource];
        self.statusLabel.text = [NSString stringWithFormat:
            @"PS3 update TAR parse failed:\n%s", updateTar.error.c_str()];
        return;
    }

    const auto packageEntry = std::find_if(
        updateTar.entries.begin(), updateTar.entries.end(), [](const auto& entry) {
            return entry.regular_file &&
                   entry.name.rfind("dev_flash_012", 0) == 0;
        });
    if (packageEntry == updateTar.entries.end() ||
        packageEntry->data_offset > updateTarData.length ||
        packageEntry->data_length > updateTarData.length -
                                      packageEntry->data_offset) {
        [handle closeFile];
        if (accessed) [url stopAccessingSecurityScopedResource];
        self.statusLabel.text = @"dev_flash_012 package is missing or invalid.";
        return;
    }

    const auto packageBegin = static_cast<std::size_t>(
        packageEntry->data_offset);
    const auto packageLength = static_cast<std::size_t>(
        packageEntry->data_length);
    const auto package = vshift::firmware::DecryptPs3ScePackage(
        std::span<const std::uint8_t>(updateTarBytes + packageBegin,
                                      packageLength));
    if (!package.ok()) {
        [handle closeFile];
        if (accessed) [url stopAccessingSecurityScopedResource];
        self.statusLabel.text = [NSString stringWithFormat:
            @"dev_flash_012 decrypt failed:\n%s", package.error.c_str()];
        return;
    }

    vshift::firmware::Ps3TarParseResult vshTar;
    const vshift::firmware::Ps3TarEntry *vshEntry = nullptr;
    for (const auto &section : package.sections) {
        auto candidate = vshift::firmware::ParsePs3Tar(section.bytes);
        if (!candidate.ok()) continue;
        const auto found = std::find_if(
            candidate.entries.begin(), candidate.entries.end(), [](const auto& entry) {
                return entry.regular_file &&
                       entry.name == "dev_flash/vsh/module/vsh.self";
            });
        if (found != candidate.entries.end()) {
            vshTar = std::move(candidate);
            vshEntry = &*std::find_if(
                vshTar.entries.begin(), vshTar.entries.end(), [](const auto& entry) {
                    return entry.regular_file &&
                           entry.name == "dev_flash/vsh/module/vsh.self";
                });
            break;
        }
    }
    [handle closeFile];
    if (accessed) [url stopAccessingSecurityScopedResource];
    if (vshEntry == nullptr) {
        self.statusLabel.text =
            @"dev_flash_012 decrypted, but vsh.self was not found.";
        return;
    }

    self.ps3FirmwareURL = url;
    self.ps3BootButton.enabled = YES;
    self.guestFrameView.image = nil;
    self.guestFrameView.hidden = YES;
    self.guestFrameStateLabel.text =
        @"PS3 VSH package prepared\nPPU/LV2/RSX framebuffer execution pending";
    NSError *manifestError = nil;
    NSDictionary *manifest = @{
        @"schema_version": @4,
        @"source_file_name": url.lastPathComponent ?: @"PS3UPDAT.PUP",
        @"source_kind": @"user-provided PS3 PUP",
        @"boot_profile": @"PS3 VSH bootstrap",
        @"firmware_version": @"4.93",
        @"package": [NSString stringWithUTF8String:packageEntry->name.c_str()],
        @"decrypted_section_count": @(package.sections.size()),
        @"vsh_self": @{
            @"path": @"dev_flash/vsh/module/vsh.self",
            @"size": @(vshEntry->data_length),
        },
        @"boot_status":
            @"dev_flash package decrypted; PPU/LV2/RSX execution pending",
    };
    const BOOL manifestSaved = [self writeManifest:manifest
                                             error:&manifestError];
    self.statusLabel.text = [NSString stringWithFormat:
        @"PS3 VSH BOOTSTRAP READY\nFirmware: 4.93\nPackage: %@\n"
         @"Decrypted sections: %lu\nvsh.self: %llu bytes\n%@",
        [NSString stringWithUTF8String:packageEntry->name.c_str()],
        static_cast<unsigned long>(package.sections.size()),
        static_cast<unsigned long long>(vshEntry->data_length),
        manifestSaved
            ? @"Next: PPU/LV2/RSX runtime and framebuffer."
            : [NSString stringWithFormat:@"Manifest save failed: %@",
                manifestError.localizedDescription ?: @"unknown error"]];
}

- (void)inspectPs3FirmwareAtURL:(NSURL *)url {
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
        fileSize < vshift::firmware::kPs3PupHeaderSize) {
        self.statusLabel.text = @"PS3 PUP is too small or unreadable.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    NSFileHandle *handle =
        [NSFileHandle fileHandleForReadingFromURL:url error:nil];
    if (handle == nil) {
        self.statusLabel.text = @"Could not open PS3UPDAT.PUP.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    NSData *fixedHeaderData =
        [handle readDataOfLength:vshift::firmware::kPs3PupHeaderSize];
    if (fixedHeaderData.length < vshift::firmware::kPs3PupHeaderSize) {
        [handle closeFile];
        self.statusLabel.text = @"Could not read the PS3 PUP header.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    const auto *fixedHeaderBytes =
        static_cast<const std::uint8_t *>(fixedHeaderData.bytes);
    const auto headerLength = ReadU64BE(fixedHeaderBytes + 0x20);
    constexpr std::uint64_t kMaximumPs3HeaderSize = 16ull * 1024ull * 1024ull;
    if (headerLength < vshift::firmware::kPs3PupHeaderSize ||
        headerLength > kMaximumPs3HeaderSize || headerLength > fileSize ||
        headerLength > std::numeric_limits<NSUInteger>::max()) {
        [handle closeFile];
        self.statusLabel.text = @"PS3 PUP header size is invalid.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    [handle seekToFileOffset:0];
    NSData *headerData =
        [handle readDataOfLength:static_cast<NSUInteger>(headerLength)];
    if (headerData.length != static_cast<NSUInteger>(headerLength)) {
        [handle closeFile];
        self.statusLabel.text = @"Could not read the PS3 PUP tables.";
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    const auto *headerBytes =
        static_cast<const std::uint8_t *>(headerData.bytes);
    const auto parsed = vshift::firmware::ParsePs3PupHeaders(
        std::span<const std::uint8_t>(headerBytes, headerData.length),
        fileSize);
    if (!parsed.ok()) {
        [handle closeFile];
        self.statusLabel.text = [NSString stringWithFormat:
            @"PS3 PUP parse failed:\n%s", parsed.error.c_str()];
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        return;
    }

    NSMutableArray *entries =
        [NSMutableArray arrayWithCapacity:parsed.entries.size()];
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
            const auto versionLength = static_cast<NSUInteger>(std::min<
                std::uint64_t>(entry.data_length, 64));
            NSData *versionData = [handle readDataOfLength:versionLength];
            firmwareVersion = [[NSString alloc] initWithData:versionData
                                                     encoding:NSUTF8StringEncoding];
            if (firmwareVersion != nil) {
                firmwareVersion = [firmwareVersion
                    stringByTrimmingCharactersInSet:
                        [NSCharacterSet whitespaceAndNewlineCharacterSet]];
            }
        }

        if (entry.data_length >= vshift::loader::kPs3SceHeaderSize) {
            [handle seekToFileOffset:entry.data_offset];
            NSData *sceData = [handle readDataOfLength:
                vshift::loader::kPs3SceHeaderSize];
            if (sceData.length == vshift::loader::kPs3SceHeaderSize) {
                const auto *sceBytes =
                    static_cast<const std::uint8_t *>(sceData.bytes);
                const auto sce = vshift::loader::ParsePs3SceHeader(
                    std::span<const std::uint8_t>(sceBytes, sceData.length),
                    entry.data_length);
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
    if (accessed) {
        [url stopAccessingSecurityScopedResource];
    }
    self.ps3FirmwareURL = url;
    self.ps3BootButton.enabled = YES;

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
    const BOOL manifestSaved = [self writeManifest:manifest
                                             error:&manifestError];
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
    [summary appendFormat:@"\n%@",
        manifestSaved
            ? @"Manifest saved. Payload decryption is not attempted."
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

    if (self.pickingPs3Firmware) {
        self.pickingPs3Firmware = NO;
        [self inspectPs3FirmwareAtURL:url];
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

    if (self.pickingRealBoot) {
        self.pickingRealBoot = NO;
        self.firmwareRootURL = url;
        [self attemptRealPS4BootAtURL:url];
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
