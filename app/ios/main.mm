#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "core/boot/synthetic_boot.h"
#include "core/cpu/arm64_jit.h"
#include "core/firmware/catalog.h"
#include "core/firmware/pup.h"
#include "core/firmware/slb2.h"
#include "core/cpu/x86_decoder.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

@interface VSHiftJITViewController : UIViewController <UIDocumentPickerDelegate>
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UIButton *exportManifestButton;
@property(nonatomic, strong) NSURL *manifestURL;
@end

@implementation VSHiftJITViewController

static std::uint32_t ReadU32LE(const std::uint8_t *bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

- (void)viewDidLoad {
    [super viewDidLoad];

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
    title.text = @"Firmware workspace";
    title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];
    title.textColor = UIColor.labelColor;
    title.adjustsFontForContentSizeCategory = YES;

    UILabel *subtitle = [[UILabel alloc] init];
    subtitle.text = @"Inspect a user-provided PUP and validate the boot path.";
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
    statusCard.layer.cornerCurve = kCACornerCurveContinuous;
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

    UIButtonConfiguration *importConfiguration =
        [UIButtonConfiguration filledButtonConfiguration];
    importConfiguration.title = @"Import PS5UPDATE.PUP";
    importConfiguration.image = [UIImage systemImageNamed:@"arrow.down.doc"];
    importConfiguration.imagePadding = 8.0;
    importConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    UIButton *importButton = [UIButton buttonWithType:UIButtonTypeSystem];
    importButton.configuration = importConfiguration;
    [importButton addTarget:self
                     action:@selector(importFirmware)
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
    firmwareCard.layer.cornerCurve = kCACornerCurveContinuous;
    firmwareCard.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);
    UIStackView *firmwareStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        firmwareHeader, importButton, self.exportManifestButton
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

    UIView *bootCard = [[UIView alloc] init];
    bootCard.backgroundColor = UIColor.secondarySystemBackgroundColor;
    bootCard.layer.cornerRadius = 24.0;
    bootCard.layer.cornerCurve = kCACornerCurveContinuous;
    bootCard.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);
    UIStackView *bootStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        bootHeader, syntheticJitButton, syntheticJitLessButton
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
