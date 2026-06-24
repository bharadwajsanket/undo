# Screenshots Directory

This directory contains visual demonstrations for the Undo repository `README.md`.

## Required Screenshots

To present a professional open-source interface, capture and place the following screenshots in this directory:

### 1. `delete-file.png`
- **Action**: Demonstrates a standard file deletion intercepted by Undo.
- **Visuals**: A terminal window executing `rm thesis.pdf` with the resulting:
  ```
  Stored in Undo
  ID: a81f92
  ```
- **Theme**: Premium terminal styling (e.g., dark mode, clean prompt).

### 2. `restore-file.png`
- **Action**: Demonstrates restoring the most recently deleted file.
- **Visuals**: A terminal window executing `undo` with the output:
  ```
  Restored thesis.pdf
  ```

### 3. `history.png`
- **Action**: Shows the transaction log history.
- **Visuals**: Execution of `undo history`, showing a tabular layout of recent deletions, timestamps, sizes, and paths.

### 4. `stats.png`
- **Action**: Shows storage disk usage and configuration statistics.
- **Visuals**: Execution of `undo stats`, highlighting the total storage size, transaction counts, compression savings ratio, and active configurations.
