"""Parse the selected S4 display driver's diagnostic Qt layer record.

The driver's depth[...] footer for the preceding video layer can share the
same line as BUF. It is not part of the following Qt layer's configuration.
"""


def qt_layer1_record(driver_text):
    import re
    records = re.findall(r"\bBUF\s+enable\s+ch\[1\]\s+lyr\[0\][^\n]*", driver_text)
    if len(records) != 1:
        raise ValueError("Exactly one enabled Qt channel1/layer0 record required")
    return records[0].strip()
