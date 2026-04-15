import re

with open('system/jlib/jfile.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

replacements = [
    (r'MakeStringException\(-1, "copyFile: target path ''%s'' could not be created"', r'MakeStringException(JLIBERR_FileCopyTargetCreateFailed, "copyFile: target path \'%s\' could not be created"'),
    (r'MakeStringException\(-1, "copySection: source ''%s'' not found"', r'MakeStringException(JLIBERR_FileCopySourceNotFound, "copySection: source \'%s\' not found"'),
    (r'MakeStringException\(-1, "renameFile: source ''%s'' not found"', r'MakeStringException(JLIBERR_FileRenameSourceNotFound, "renameFile: source \'%s\' not found"'),
    (r'MakeStringException\(-1, "renameFile: source ''%s'' is not a valid file"', r'MakeStringException(JLIBERR_FileRenameSourceInvalid, "renameFile: source \'%s\' is not a valid file"'),
    (r'MakeStringException\(-1, "renameFile: source ''%s'' is readonly"', r'MakeStringException(JLIBERR_FileRenameSourceReadOnly, "renameFile: source \'%s\' is readonly"'),
    (r'MakeStringException\(-1, "renameFile: target path ''%s'' could not be created"', r'MakeStringException(JLIBERR_FileRenameTargetCreateFailed, "renameFile: target path \'%s\' could not be created"'),
    (r'MakeStringException\(-1, "renameFile: target file already exists: ''%s'' will not overwrite"', r'MakeStringException(JLIBERR_FileRenameTargetExists, "renameFile: target file already exists: \'%s\' will not overwrite"'),
    (r'MakeStringException\(-1, "copyFile: source ''%s'' not found"', r'MakeStringException(JLIBERR_FileCopySourceNotFound, "copyFile: source \'%s\' not found"'),
    (r'MakeStringException\(-1, "extractBlobElements: file ''%s'' not found"', r'MakeStringException(JLIBERR_ExtractBlobElementsFileNotFound, "extractBlobElements: file \'%s\' not found"'),
]

# We need to modify regex since the quotes are sometimes around the `%s` or outside
# Instead of strict matching, just match by substring using str.replace

replacements_str = [
    ('MakeStringException(-1, "copyFile: target path \'%s\' could not be created"', 'MakeStringException(JLIBERR_FileCopyTargetCreateFailed, "copyFile: target path \'%s\' could not be created"'),
    ('MakeStringException(-1, "copySection: source \'%s\' not found"', 'MakeStringException(JLIBERR_FileCopySourceNotFound, "copySection: source \'%s\' not found"'),
    ('MakeStringException(-1, "renameFile: source \'%s\' not found"', 'MakeStringException(JLIBERR_FileRenameSourceNotFound, "renameFile: source \'%s\' not found"'),
    ('MakeStringException(-1, "renameFile: source \'%s\' is not a valid file"', 'MakeStringException(JLIBERR_FileRenameSourceInvalid, "renameFile: source \'%s\' is not a valid file"'),
    ('MakeStringException(-1, "renameFile: source \'%s\' is readonly"', 'MakeStringException(JLIBERR_FileRenameSourceReadOnly, "renameFile: source \'%s\' is readonly"'),
    ('MakeStringException(-1, "renameFile: target path \'%s\' could not be created"', 'MakeStringException(JLIBERR_FileRenameTargetCreateFailed, "renameFile: target path \'%s\' could not be created"'),
    ('MakeStringException(-1, "renameFile: target file already exists: \'%s\' will not overwrite"', 'MakeStringException(JLIBERR_FileRenameTargetExists, "renameFile: target file already exists: \'%s\' will not overwrite"'),
    ('MakeStringException(-1, "copyFile: source \'%s\' not found"', 'MakeStringException(JLIBERR_FileCopySourceNotFound, "copyFile: source \'%s\' not found"'),
    ('MakeStringException(-1, "extractBlobElements: file \'%s\' not found"', 'MakeStringException(JLIBERR_ExtractBlobElementsFileNotFound, "extractBlobElements: file \'%s\' not found"'),
]

for s, r in replacements_str:
    content = content.replace(s, r)

with open('system/jlib/jfile.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
