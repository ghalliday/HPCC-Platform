import re

with open('system/jlib/jerror.hpp', 'r', encoding='utf-8') as f:
    content = f.read()

new_defines = """
#define JLIBERR_UtilJcontainerizedErrMsg                6300
#define JLIBERR_UtilJcontainerizedExceptionText         6301
#define JLIBERR_SystemMsgStr                            6302
#define JLIBERR_UtilSStr                                6303
#define JLIBERR_CompressUnexpectedZeroLengthCompressionBlock 6304
"""

content = content.replace('//---- Text for all errors', f'{new_defines}\n//---- Text for all errors')

with open('system/jlib/jerror.hpp', 'w', encoding='utf-8') as f:
    f.write(content)
