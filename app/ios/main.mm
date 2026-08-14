#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "core/frontend/media_store.h"
#include "core/ps3/firmware_installer.h"
#include "core/ps3/rpcs3_core.h"

#include <filesystem>
#include <memory>
#include <string>

namespace {

NSString *StringFromUTF8(const std::string &value) {
    return [NSString stringWithUTF8String:value.c_str()] ?: @"";
}

NSString *MediaKindName(vshift::frontend::MediaKind kind) {
    switch (kind) {
    case vshift::frontend::MediaKind::Music:
        return @"music";
    case vshift::frontend::MediaKind::Photo:
        return @"photo";
    case vshift::frontend::MediaKind::Video:
        return @"video";
    case vshift::frontend::MediaKind::Unknown:
        return @"USB/imported";
    }
    return @"file";
}

} // namespace

typedef NS_ENUM(NSInteger, VSHiftPickerMode) {
    VSHiftPickerModeFirmware = 1,
    VSHiftPickerModeMedia = 2,
    VSHiftPickerModeIso = 3,
};

@interface VSHiftViewController : UIViewController <UIDocumentPickerDelegate> {
    std::shared_ptr<vshift::ps3::Rpcs3Core> _core;
}

@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UILabel *guestFrameStateLabel;
@property(nonatomic, strong) UIImageView *guestFrameView;
@property(nonatomic, strong) UIButton *startButton;
@property(nonatomic, strong) UIButton *drawerButton;
@property(nonatomic, strong) UIView *controlDrawer;
@property(nonatomic, strong) NSURL *workspaceURL;
@property(nonatomic, assign) VSHiftPickerMode pickerMode;
@property(nonatomic, assign) BOOL drawerOpen;
@property(nonatomic, assign) BOOL fullscreen;
@property(nonatomic, assign) BOOL coreOperationInFlight;

@end

@implementation VSHiftViewController

- (UIButton *)buttonWithTitle:(NSString *)title
                         image:(NSString *)imageName
                         action:(SEL)action
                          tinted:(BOOL)tinted {
    UIButtonConfiguration *configuration = tinted
        ? [UIButtonConfiguration tintedButtonConfiguration]
        : [UIButtonConfiguration filledButtonConfiguration];
    configuration.title = title;
    configuration.image = [UIImage systemImageNamed:imageName];
    configuration.imagePadding = 8.0;
    configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;

    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    button.configuration = configuration;
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (UIView *)card {
    UIView *view = [[UIView alloc] init];
    view.backgroundColor = UIColor.secondarySystemBackgroundColor;
    view.layer.cornerRadius = 24.0;
    view.layoutMargins = UIEdgeInsetsMake(16.0, 16.0, 16.0, 16.0);
    return view;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.systemBackgroundColor;

    UIScrollView *scrollView = [[UIScrollView alloc] init];
    scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    scrollView.alwaysBounceVertical = YES;
    [self.view addSubview:scrollView];

    UIStackView *content = [[UIStackView alloc] init];
    content.translatesAutoresizingMaskIntoConstraints = NO;
    content.axis = UILayoutConstraintAxisVertical;
    content.spacing = 16.0;
    content.layoutMargins = UIEdgeInsetsMake(20.0, 16.0, 28.0, 16.0);
    content.layoutMarginsRelativeArrangement = YES;
    [scrollView addSubview:content];

    UILabel *eyebrow = [[UILabel alloc] init];
    eyebrow.text = @"VSHIFT LAB · PS3";
    eyebrow.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    eyebrow.textColor = UIColor.secondaryLabelColor;

    UILabel *title = [[UILabel alloc] init];
    title.text = @"PS3 VSH workspace";
    title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];
    title.adjustsFontForContentSizeCategory = YES;

    UILabel *subtitle = [[UILabel alloc] init];
    subtitle.text = @"Install a user-provided PS3UPDAT.PUP, start the real RPCS3 VSH path, and manage its virtual storage.";
    subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    subtitle.textColor = UIColor.secondaryLabelColor;
    subtitle.numberOfLines = 0;
    subtitle.adjustsFontForContentSizeCategory = YES;

    UIStackView *header = [[UIStackView alloc] initWithArrangedSubviews:@[
        eyebrow, title, subtitle
    ]];
    header.axis = UILayoutConstraintAxisVertical;
    header.spacing = 4.0;

    UIView *statusCard = [self card];
    UILabel *statusTitle = [[UILabel alloc] init];
    statusTitle.text = @"Runtime status";
    statusTitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];

    self.statusLabel = [[UILabel alloc] init];
    self.statusLabel.text = @"PS3 firmware is not installed yet.";
    self.statusLabel.textColor = UIColor.secondaryLabelColor;
    self.statusLabel.numberOfLines = 0;
    self.statusLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
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

    self.guestFrameView = [[UIImageView alloc] init];
    self.guestFrameView.translatesAutoresizingMaskIntoConstraints = NO;
    self.guestFrameView.contentMode = UIViewContentModeScaleAspectFit;
    self.guestFrameView.hidden = YES;
    [displayCard addSubview:self.guestFrameView];

    self.guestFrameStateLabel = [[UILabel alloc] init];
    self.guestFrameStateLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.guestFrameStateLabel.text = @"Real PS3 RSX frame is not connected yet\nNo synthetic XMB is shown";
    self.guestFrameStateLabel.textColor = UIColor.whiteColor;
    self.guestFrameStateLabel.textAlignment = NSTextAlignmentCenter;
    self.guestFrameStateLabel.numberOfLines = 0;
    self.guestFrameStateLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    [displayCard addSubview:self.guestFrameStateLabel];

    self.drawerButton = [UIButton buttonWithType:UIButtonTypeSystem];
    self.drawerButton.translatesAutoresizingMaskIntoConstraints = NO;
    self.drawerButton.tintColor = UIColor.whiteColor;
    self.drawerButton.backgroundColor = [UIColor colorWithWhite:0.18 alpha:0.92];
    self.drawerButton.layer.cornerRadius = 22.0;
    [self.drawerButton setImage:[UIImage systemImageNamed:@"chevron.left"] forState:UIControlStateNormal];
    [self.drawerButton addTarget:self action:@selector(toggleDrawer) forControlEvents:UIControlEventTouchUpInside];
    [displayCard addSubview:self.drawerButton];

    self.controlDrawer = [[UIView alloc] init];
    self.controlDrawer.translatesAutoresizingMaskIntoConstraints = NO;
    self.controlDrawer.backgroundColor = [UIColor colorWithWhite:0.08 alpha:0.96];
    self.controlDrawer.layer.cornerRadius = 20.0;
    self.controlDrawer.hidden = YES;
    [displayCard addSubview:self.controlDrawer];

    UILabel *drawerTitle = [[UILabel alloc] init];
    drawerTitle.text = @"Console actions";
    drawerTitle.textColor = UIColor.whiteColor;
    drawerTitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    UIButton *pause = [self buttonWithTitle:@"Pause / resume" image:@"pause.circle" action:@selector(togglePause) tinted:YES];
    UIButton *drawerMedia = [self buttonWithTitle:@"Import media" image:@"square.and.arrow.down" action:@selector(importMedia) tinted:YES];
    UIButton *drawerIso = [self buttonWithTitle:@"Mount ISO" image:@"opticaldisc" action:@selector(mountISO) tinted:YES];
    UIButton *drawerSettings = [self buttonWithTitle:@"Console settings" image:@"slider.horizontal.3" action:@selector(openSettings) tinted:YES];
    UIStackView *drawerStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        drawerTitle, pause, drawerMedia, drawerIso, drawerSettings
    ]];
    drawerStack.translatesAutoresizingMaskIntoConstraints = NO;
    drawerStack.axis = UILayoutConstraintAxisVertical;
    drawerStack.spacing = 10.0;
    [self.controlDrawer addSubview:drawerStack];

    UIView *firmwareCard = [self card];
    UILabel *firmwareHeader = [[UILabel alloc] init];
    firmwareHeader.text = @"FIRMWARE";
    firmwareHeader.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    firmwareHeader.textColor = UIColor.secondaryLabelColor;
    UIButton *importFirmware = [self buttonWithTitle:@"Import PS3UPDAT.PUP and install" image:@"shippingbox" action:@selector(importFirmware) tinted:NO];
    self.startButton = [self buttonWithTitle:@"Start real PS3 VSH" image:@"play.fill" action:@selector(startVSH) tinted:NO];
    self.startButton.enabled = NO;
    UIButton *stopButton = [self buttonWithTitle:@"Stop emulator" image:@"stop.fill" action:@selector(stopVSH) tinted:YES];
    UIStackView *firmwareStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        firmwareHeader, importFirmware, self.startButton, stopButton
    ]];
    firmwareStack.translatesAutoresizingMaskIntoConstraints = NO;
    firmwareStack.axis = UILayoutConstraintAxisVertical;
    firmwareStack.spacing = 10.0;
    [firmwareCard addSubview:firmwareStack];

    UIView *storageCard = [self card];
    UILabel *storageHeader = [[UILabel alloc] init];
    storageHeader.text = @"VIRTUAL STORAGE";
    storageHeader.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    storageHeader.textColor = UIColor.secondaryLabelColor;
    UIButton *mediaButton = [self buttonWithTitle:@"Import music / photos / video" image:@"photo.on.rectangle" action:@selector(importMedia) tinted:YES];
    UIButton *isoButton = [self buttonWithTitle:@"Mount PS3 ISO" image:@"opticaldisc" action:@selector(mountISO) tinted:YES];
    UILabel *storageNote = [[UILabel alloc] init];
    storageNote.text = @"Files are copied into the sandbox-backed dev_hdd0 or dev_usb000 layout. ISO insertion uses RPCS3's disc path when VSH is running.";
    storageNote.textColor = UIColor.secondaryLabelColor;
    storageNote.numberOfLines = 0;
    storageNote.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    UIStackView *storageStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        storageHeader, mediaButton, isoButton, storageNote
    ]];
    storageStack.translatesAutoresizingMaskIntoConstraints = NO;
    storageStack.axis = UILayoutConstraintAxisVertical;
    storageStack.spacing = 10.0;
    [storageCard addSubview:storageStack];

    [content addArrangedSubview:header];
    [content addArrangedSubview:statusCard];
    [content addArrangedSubview:displayCard];
    [content addArrangedSubview:firmwareCard];
    [content addArrangedSubview:storageCard];

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
        [displayCard.heightAnchor constraintEqualToConstant:260.0],
        [self.guestFrameView.leadingAnchor constraintEqualToAnchor:displayCard.leadingAnchor],
        [self.guestFrameView.trailingAnchor constraintEqualToAnchor:displayCard.trailingAnchor],
        [self.guestFrameView.topAnchor constraintEqualToAnchor:displayCard.topAnchor],
        [self.guestFrameView.bottomAnchor constraintEqualToAnchor:displayCard.bottomAnchor],
        [self.guestFrameStateLabel.leadingAnchor constraintEqualToAnchor:displayCard.leadingAnchor constant:20.0],
        [self.guestFrameStateLabel.trailingAnchor constraintEqualToAnchor:displayCard.trailingAnchor constant:-20.0],
        [self.guestFrameStateLabel.centerYAnchor constraintEqualToAnchor:displayCard.centerYAnchor],
        [self.drawerButton.trailingAnchor constraintEqualToAnchor:displayCard.trailingAnchor constant:-12.0],
        [self.drawerButton.centerYAnchor constraintEqualToAnchor:displayCard.centerYAnchor],
        [self.drawerButton.widthAnchor constraintEqualToConstant:44.0],
        [self.drawerButton.heightAnchor constraintEqualToConstant:44.0],
        [self.controlDrawer.trailingAnchor constraintEqualToAnchor:displayCard.trailingAnchor constant:-8.0],
        [self.controlDrawer.topAnchor constraintEqualToAnchor:displayCard.topAnchor constant:8.0],
        [self.controlDrawer.bottomAnchor constraintEqualToAnchor:displayCard.bottomAnchor constant:-8.0],
        [self.controlDrawer.widthAnchor constraintEqualToConstant:228.0],
        [drawerStack.leadingAnchor constraintEqualToAnchor:self.controlDrawer.leadingAnchor constant:12.0],
        [drawerStack.trailingAnchor constraintEqualToAnchor:self.controlDrawer.trailingAnchor constant:-12.0],
        [drawerStack.topAnchor constraintEqualToAnchor:self.controlDrawer.topAnchor constant:12.0],
        [drawerStack.bottomAnchor constraintLessThanOrEqualToAnchor:self.controlDrawer.bottomAnchor constant:-12.0],
        [firmwareStack.leadingAnchor constraintEqualToAnchor:firmwareCard.layoutMarginsGuide.leadingAnchor],
        [firmwareStack.trailingAnchor constraintEqualToAnchor:firmwareCard.layoutMarginsGuide.trailingAnchor],
        [firmwareStack.topAnchor constraintEqualToAnchor:firmwareCard.layoutMarginsGuide.topAnchor],
        [firmwareStack.bottomAnchor constraintEqualToAnchor:firmwareCard.layoutMarginsGuide.bottomAnchor],
        [storageStack.leadingAnchor constraintEqualToAnchor:storageCard.layoutMarginsGuide.leadingAnchor],
        [storageStack.trailingAnchor constraintEqualToAnchor:storageCard.layoutMarginsGuide.trailingAnchor],
        [storageStack.topAnchor constraintEqualToAnchor:storageCard.layoutMarginsGuide.topAnchor],
        [storageStack.bottomAnchor constraintEqualToAnchor:storageCard.layoutMarginsGuide.bottomAnchor],
    ]];

    NSError *workspaceError = nil;
    self.workspaceURL = [self createWorkspace:&workspaceError];
    if (!self.workspaceURL) {
        self.statusLabel.text = [NSString stringWithFormat:
            @"Could not prepare PS3 storage: %@", workspaceError.localizedDescription ?: @"unknown error"];
    } else {
        [self refreshFirmwareState];
    }
}

- (NSURL *)createWorkspace:(NSError **)error {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *applicationSupport = [fileManager URLForDirectory:NSApplicationSupportDirectory
                                                    inDomain:NSUserDomainMask
                                           appropriateForURL:nil
                                                      create:YES
                                                       error:error];
    if (!applicationSupport) {
        return nil;
    }
    NSURL *workspace = [applicationSupport URLByAppendingPathComponent:@"VSHift/PS3"
                                                              isDirectory:YES];
    if (![fileManager createDirectoryAtURL:workspace
               withIntermediateDirectories:YES
                                attributes:nil
                                     error:error]) {
        return nil;
    }
    return workspace;
}

- (void)refreshFirmwareState {
    if (!self.workspaceURL) {
        return;
    }
    NSURL *vshURL = [self.workspaceURL URLByAppendingPathComponent:@"dev_flash/vsh/module/vsh.self"];
    BOOL installed = [[NSFileManager defaultManager] fileExistsAtPath:vshURL.path];
    self.startButton.enabled = installed && _core == nullptr;
    if (installed && _core == nullptr) {
        self.statusLabel.text = @"PS3 dev_flash is installed. The real VSH boot is ready to try.";
    }
}

- (void)presentPicker:(VSHiftPickerMode)mode {
    self.pickerMode = mode;
    NSArray<UTType *> *types = nil;
    if (mode == VSHiftPickerModeMedia) {
        types = @[ UTTypeAudio, UTTypeImage, UTTypeMovie, UTTypeData ];
    } else {
        types = @[ UTTypeData ];
    }
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)importFirmware {
    [self presentPicker:VSHiftPickerModeFirmware];
}

- (void)importMedia {
    [self presentPicker:VSHiftPickerModeMedia];
}

- (void)mountISO {
    [self presentPicker:VSHiftPickerModeIso];
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller
 didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    (void)controller;
    NSURL *url = urls.firstObject;
    if (!url) {
        return;
    }
    switch (self.pickerMode) {
    case VSHiftPickerModeFirmware:
        [self installFirmwareAtURL:url];
        break;
    case VSHiftPickerModeMedia:
        [self importMediaAtURL:url];
        break;
    case VSHiftPickerModeIso:
        [self mountISOAtURL:url];
        break;
    }
}

- (void)installFirmwareAtURL:(NSURL *)url {
    if (!self.workspaceURL) {
        return;
    }
    NSError *workspaceError = nil;
    NSURL *workspace = self.workspaceURL ?: [self createWorkspace:&workspaceError];
    if (!workspace) {
        self.statusLabel.text = @"PS3 storage is unavailable.";
        return;
    }
    const BOOL accessed = [url startAccessingSecurityScopedResource];
    std::filesystem::path pupPath(url.path.fileSystemRepresentation);
    std::filesystem::path rootPath(workspace.path.fileSystemRepresentation);
    self.statusLabel.text = @"Installing PS3 firmware with RPCS3 PUP/SELF/TAR code…";
    self.startButton.enabled = NO;

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        vshift::ps3::Rpcs3FirmwareInstaller installer;
        const auto report = installer.Install(pupPath, rootPath);
        if (accessed) {
            [url stopAccessingSecurityScopedResource];
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!report.ok()) {
                self.statusLabel.text = [NSString stringWithFormat:
                    @"PS3 firmware install failed:\n%s", report.error.c_str()];
                return;
            }
            self.statusLabel.text = [NSString stringWithFormat:
                @"PS3 firmware installed\nVersion: %s\nPackages: %llu\nStarting VSH is the next step.",
                report.firmware_version.c_str(),
                static_cast<unsigned long long>(report.package_count)];
            self.startButton.enabled = YES;
            [self startVSH];
        });
    });
}

- (void)startVSH {
    if (self.coreOperationInFlight) {
        self.statusLabel.text = @"Another PS3 operation is still in progress.";
        return;
    }
    if (_core && _core->running()) {
        self.statusLabel.text = @"PS3 VSH is already running.";
        return;
    }
    if (!self.workspaceURL) {
        self.statusLabel.text = @"Import and install PS3 firmware first.";
        return;
    }
    NSURL *vshURL = [self.workspaceURL URLByAppendingPathComponent:@"dev_flash/vsh/module/vsh.self"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:vshURL.path]) {
        self.statusLabel.text = @"dev_flash/vsh/module/vsh.self was not found. Install PS3UPDAT.PUP first.";
        return;
    }

    std::filesystem::path rootPath(self.workspaceURL.path.fileSystemRepresentation);
    __weak VSHiftViewController *weakSelf = self;
    _core = std::make_shared<vshift::ps3::Rpcs3Core>(vshift::ps3::CoreCallbacks{
        .on_started = [weakSelf]() {
            dispatch_async(dispatch_get_main_queue(), ^{
                VSHiftViewController *strongSelf = weakSelf;
                if (!strongSelf) return;
                strongSelf.statusLabel.text = @"VSH boot path started. Waiting for a real RPCS3 RSX frame; XMB is not emulated by the frontend.";
                strongSelf.guestFrameStateLabel.text = @"VSH started\nWaiting for real RSX video output";
                strongSelf.drawerButton.hidden = NO;
            });
        },
        .on_stopped = [weakSelf]() {
            dispatch_async(dispatch_get_main_queue(), ^{
                VSHiftViewController *strongSelf = weakSelf;
                if (!strongSelf) return;
                strongSelf.statusLabel.text = @"PS3 emulator stopped.";
                strongSelf.guestFrameStateLabel.text = @"Real PS3 RSX frame is not connected yet\nNo synthetic XMB is shown";
                strongSelf.startButton.enabled = YES;
            });
        },
        .on_log = [](std::string) {},
        .call_from_main_thread = [](std::function<void()> function) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (function) {
                    function();
                }
            });
        },
    });
    const auto core = _core;
    self.statusLabel.text = @"Initializing RPCS3 PS3 core and mounting dev_flash…";
    self.startButton.enabled = NO;
    self.coreOperationInFlight = YES;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        const auto report = core->StartVsh(rootPath);
        dispatch_async(dispatch_get_main_queue(), ^{
            VSHiftViewController *strongSelf = weakSelf;
            if (!strongSelf) return;
            strongSelf.coreOperationInFlight = NO;
            if (!report.ok()) {
                strongSelf.statusLabel.text = [NSString stringWithFormat:
                    @"Real PS3 VSH start failed:\n%s", report.error.c_str()];
                strongSelf.startButton.enabled = YES;
                strongSelf->_core.reset();
            }
        });
    });
}

- (void)stopVSH {
    if (self.coreOperationInFlight) {
        self.statusLabel.text = @"Another PS3 operation is still in progress.";
        return;
    }
    if (!_core) {
        self.statusLabel.text = @"PS3 emulator is not running.";
        return;
    }
    const auto core = _core;
    self.statusLabel.text = @"Stopping RPCS3…";
    self.coreOperationInFlight = YES;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        core->Stop();
        dispatch_async(dispatch_get_main_queue(), ^{
            self.coreOperationInFlight = NO;
            self->_core.reset();
            [self refreshFirmwareState];
        });
    });
}

- (void)togglePause {
    if (self.coreOperationInFlight) {
        self.statusLabel.text = @"Another PS3 operation is still in progress.";
        return;
    }
    if (!_core || !_core->running()) {
        self.statusLabel.text = @"Start the real PS3 VSH before using pause.";
        return;
    }
    const auto core = _core;
    self.coreOperationInFlight = YES;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        const BOOL paused = core->Pause();
        if (!paused) {
            core->Resume();
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            self.coreOperationInFlight = NO;
            self.statusLabel.text = paused ? @"PS3 VSH paused." : @"PS3 VSH resumed.";
        });
    });
}

- (void)importMediaAtURL:(NSURL *)url {
    if (!self.workspaceURL) return;
    const BOOL accessed = [url startAccessingSecurityScopedResource];
    std::filesystem::path source(url.path.fileSystemRepresentation);
    std::filesystem::path root(self.workspaceURL.path.fileSystemRepresentation);
    self.statusLabel.text = @"Importing file into the PS3 virtual storage…";
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        const auto report = vshift::frontend::ImportMediaFile(source, root);
        if (accessed) [url stopAccessingSecurityScopedResource];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!report.ok()) {
                self.statusLabel.text = [NSString stringWithFormat:
                    @"Import failed:\n%s", report.error.c_str()];
                return;
            }
            self.statusLabel.text = [NSString stringWithFormat:
                @"Imported %@ into the virtual console:\n%@",
                MediaKindName(report.kind),
                StringFromUTF8(report.destination.string())];
        });
    });
}

- (void)mountISOAtURL:(NSURL *)url {
    if (self.coreOperationInFlight) {
        self.statusLabel.text = @"Another PS3 operation is still in progress.";
        return;
    }
    if (!_core || !_core->running()) {
        self.statusLabel.text = @"Start the real PS3 VSH before mounting an ISO.";
        return;
    }
    const BOOL accessed = [url startAccessingSecurityScopedResource];
    std::filesystem::path iso(url.path.fileSystemRepresentation);
    const auto core = _core;
    self.statusLabel.text = @"Asking RPCS3 to insert the selected ISO…";
    self.coreOperationInFlight = YES;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        const BOOL mounted = core->MountIso(iso);
        if (accessed) [url stopAccessingSecurityScopedResource];
        dispatch_async(dispatch_get_main_queue(), ^{
            self.coreOperationInFlight = NO;
            self.statusLabel.text = mounted
                ? @"ISO mounted through RPCS3 dev_bdvd."
                : @"RPCS3 could not recognize or mount this ISO.";
        });
    });
}

- (void)toggleDrawer {
    self.drawerOpen = !self.drawerOpen;
    self.controlDrawer.hidden = !self.drawerOpen;
    NSString *symbol = self.drawerOpen ? @"chevron.right" : @"chevron.left";
    [self.drawerButton setImage:[UIImage systemImageNamed:symbol] forState:UIControlStateNormal];
}

- (void)openSettings {
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"PS3 console settings"
                                                                       message:@"PS3 RAM is fixed by the console hardware. These UTM-style values control the host bridge and are saved for the upcoming RSX/audio adapters."
                                                                preferredStyle:UIAlertControllerStyleAlert];
    [alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
        field.placeholder = @"Resolution scale (1–3)";
        field.keyboardType = UIKeyboardTypeNumberPad;
        field.text = [[NSUserDefaults standardUserDefaults] stringForKey:@"resolutionScale"] ?: @"1";
    }];
    [alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
        field.placeholder = @"Audio buffer (ms)";
        field.keyboardType = UIKeyboardTypeNumberPad;
        field.text = [[NSUserDefaults standardUserDefaults] stringForKey:@"audioBufferMs"] ?: @"80";
    }];
    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Save" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        (void)action;
        NSArray<UITextField *> *fields = alert.textFields;
        [[NSUserDefaults standardUserDefaults] setObject:fields[0].text ?: @"1" forKey:@"resolutionScale"];
        [[NSUserDefaults standardUserDefaults] setObject:fields[1].text ?: @"80" forKey:@"audioBufferMs"];
        [[NSUserDefaults standardUserDefaults] synchronize];
        self.statusLabel.text = @"Host settings saved. Renderer/audio values will be applied by their native adapters.";
    }]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (BOOL)prefersStatusBarHidden {
    return self.fullscreen;
}

- (void)dealloc {
    if (_core) {
        _core->Stop();
        _core.reset();
    }
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
    self.window.rootViewController = [[VSHiftViewController alloc] init];
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
