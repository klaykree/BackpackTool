import re

FunctionFile = open("function.txt")

FunctionFileText = FunctionFile.read()

FormattedText = "{"

#{ 0x8B, true },	{ 0xFF, true },							//MOV EDI, EDI

length = len(FunctionFileText)
i = 0
while i < len(FunctionFileText):
    SpaceIndex = FunctionFileText.find(' ', i)
    FormattedText += "\n\t"
    for j in range(i, SpaceIndex, 2):
        FormattedText += "{ 0x" + FunctionFileText[j] + FunctionFileText[j + 1] + ", true }, "
    DesciptionIndex = re.search(r"\s[a-zA-Z]", FunctionFileText[SpaceIndex:]).start() + SpaceIndex + 1
    i = FunctionFileText.find('\n', i)
    if i == -1:
        break
    FormattedText += "//"
    FormattedText += FunctionFileText[DesciptionIndex:i]
    i += 1

FormattedText += "};"

print(FormattedText)

FunctionFile.close()

FormattedFunctionFile = open("formattedfunction.txt", 'w')

FormattedFunctionFile.write(FormattedText)

FormattedFunctionFile.close()