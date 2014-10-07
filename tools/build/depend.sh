#!/bin/bash

# usage: ./depend.sh filename buildfolder
# If the file extension is *.cpp, it is considered as an implementation file,
# else it is considered as a header file.
# The script produces on stdout a set of dependencies in GNU make format.

# The dependencies of the file $foo.{hpp|cpp} are obtained by looking  
# in the source code. For each included file, say $bar.hpp,
#   - If $foo has the extension hpp
#        then "$foo.p" depends on "$bar.p"
#   - Else if $foo has the extension cpp, 
#        then "$foo.o" depends on "$bar.p".
#   - In both case, if $foo is not the same as $bar, we have:
#         "$foo.ok" depends on "$bar.ok".

# If you have a line that contains an include for which you do not want
# to generated a dependency, add the string "nodepend" inside a comment
# on the same line as the include. Alternatively, place a double-space
# after the keyword "#include".
   

# -----get arguments

FILE=$1
BUILD=$2

FOLDER=`dirname ${FILE}`
EXT="${FILE##*.}"
FILE_NO_EXT="${FILE%.*}"
BASE=`basename ${FILE_NO_EXT}`


# -----extract dependencies

ALL=
INCLUDE_PATTERN="#include \"([^\"]*)\""
while read LINE; do
  if [[ $LINE == *nodepend* ]]; then 
    continue
  fi
  if [[ $LINE =~ $INCLUDE_PATTERN ]]; then
    n=${#BASH_REMATCH[*]}
    i=1
    while [[ $i -lt $n ]]; do
      DEP=${BASH_REMATCH[$i]}
      # echo "dependency: ${DEP}"
      ALL="${ALL} ${DEP}"
      let i++
    done
  fi
done < $FILE

# echo "#DEPENDENCIES= ${ALL}"

# -----produce output

for DEP in $ALL; do  
  DEP_NO_EXT=${DEP%.*}
  if [ "${EXT}" == "cpp" ]; then
    echo "${BUILD}/${BASE}.o: ${BUILD}/${DEP_NO_EXT}.p"
  else
    echo "${BUILD}/${BASE}.p: ${BUILD}/${DEP_NO_EXT}.p"
  fi
  if [ "${BASE}" != "${DEP_NO_EXT}" ]; then
    echo "${BUILD}/${BASE}.ok: ${BUILD}/${DEP_NO_EXT}.ok"
  fi

done
