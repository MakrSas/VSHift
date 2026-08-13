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
