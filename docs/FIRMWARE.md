# Firmware boundary

VSHift does not distribute `PS5UPDATE.PUP`, Sony keys, decrypted system files,
copyrighted assets, or proprietary libraries. The intended interface is a user
selected official firmware file supplied at runtime.

The planned firmware manager will:

1. validate the selected file and identify its version;
2. extract only components required by the selected boot profile;
3. store a versioned cache in Application Support;
4. expose version, size, status, and deletion controls;
5. never put firmware content in the IPA.

The exact PUP/SELF cryptographic and packaging details are not assumed here.
They must be implemented only after the legal input boundary and the required
user-supplied material are clear.

## First inspection milestone

PS5 update packages use an outer `SLB2` container. The read-only
`vshift_firmware_inspect` tool parses only the header and file table, validates
that every listed file stays inside the user-selected container, and prints
component names, offsets, and sizes. It does not decrypt, extract, modify, or
ship firmware data.

Example:

```text
vshift_firmware_inspect PS5UPDATE.PUP
```

This is intentionally the first real-firmware test. The next step is to add an
iOS document picker and persist the resulting manifest, before attempting to
read any encrypted component.
