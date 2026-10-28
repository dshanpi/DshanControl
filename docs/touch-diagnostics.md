# Offline touch diagnostic tools

`scripts/touch_diagnostics.py` checks a saved diagnostic log or Python source on
an Ubuntu host. It never opens a serial connection, sends a command, executes the
source being checked, changes driver settings, or retries a previous operation.
It accepts bounded regular files and rejects symlinks, devices and pipes.

## Inspect a saved log

```sh
python3 scripts/touch_diagnostics.py inspect saved-uart.log --prefix DET --finalized
```

Use the exact marker prefix expected for that transaction. `--finalized` means
that the archive is sealed and no further bytes can arrive; it permits a final
root prompt without a newline. Omit it for an incomplete receive snapshot.
Do not use it to turn an arbitrary live buffer into a completed transaction.

The parser recognizes Python errors, including `SyntaxError` and qualified
`binascii.Error`, followed by a root prompt. A prompt can share a line with a
kernel timestamp. A prompt alone does not prove that source was executed.
An error result does not establish whether the source started executing.

Frames use `PREFIX_META <raw-size> <sha256> <base64-size>`, one or more
`PREFIX_DATA <base64>` lines, and `PREFIX_END`, followed by a root prompt.
The parser checks record order, duplicate or damaged markers, exact lengths,
strict base64, bounded decompression, absence of trailing compressed data, and
SHA-256. It does not repair damaged payloads or infer missing data.
Separate kernel log lines may occur between records; noise inserted inside a
record is rejected. Total input is limited to 1,000,000 bytes, decoded JSON to
262,144 bytes, and encoded frame data to 524,288 bytes.

Exit codes:

| Code | Meaning |
| --- | --- |
| 0 | Frame integrity verified, or source preflight passed |
| 2 | A Python error and later root prompt were observed |
| 3 | Waiting for enough output to classify the transaction |
| 4 | Invalid input or failed integrity/preflight check |

Every log result keeps `retry_allowed` and `board_accepted` false. A complete
frame is only transport evidence: it cannot establish current boot identity,
process authority, hardware health, successful physical input, or permission to
run another command. This offline tool is not connected to a live controller.

## Check source without running it

```sh
python3 scripts/touch_diagnostics.py preflight diagnostic.py
```

Preflight compiles the source locally, checks Python 3.7 syntax using the host
parser, and verifies compression/base64 and shell-quoting round trips. Both the
wrapper and complete shell command must fit within 3,500 ASCII bytes. Source
input is limited to 65,536 bytes. The output contains sizes and the source SHA,
not an executable command. A successful local check does not prove target
runtime compatibility or explain corruption seen over a real serial channel.

`public_bytes(data)` preserves a public proc/sysfs field as base64 with its full
SHA-256, including invalid UTF-8 bytes, up to 32,768 bytes. It does not read paths
or collect private environment/configuration data. Callers remain responsible
for selecting public fields and checking identity, permissions and read bounds.

## Validation and limits

```sh
python3 -m unittest discover -s tests -p test_touch_diagnostics.py -v
```

The portable suite uses synthetic fixtures only. It covers noise, partial
buffers, failure terminals, byte preservation, damaged/oversized frames,
non-execution during preflight, and CLI input/exit-code boundaries. These tests
also run through `make test`.

Physical touch remains unverified. Wiring and board revision must be confirmed
before fresh, authorized device observations and actual tap-response validation.
Board-specific scripts, device identities, private configurations, raw device
logs and SDK/vendor files are deliberately absent from this source change.
