BUILD_TOOLS=`dirname $0`
BUILD_FOLDER=$1
COMPILESH=$2
SOURCE_BASE=$3
OUTPUT=$4
VERBOSE=$5

# This script compiles a program using the compilation script 
# ${BUILD_FOLDER}/${COMPILESH}, by passing it as argument an 
# appropriate list of *.o files. This list is obtained by 
# investigating the *.d files that contain the dependencies
# on the *.ok files, starting from ${SOURCE_BASE}.ok

# It performs a depth-first search, implemented in an auxiliary php program.

SOURCE_OBJ=${BUILD_FOLDER}/${SOURCE_BASE}.o

echo "php ${BUILD_TOOLS}/filterobjdfs.php ${BUILD_FOLDER} ${SOURCE_BASE}"
FILTERED_OBJ=`php ${BUILD_TOOLS}/filterobjdfs.php ${BUILD_FOLDER} ${SOURCE_BASE}`

RESULT=$?
if [ ${RESULT} -ne 0 ]; then
    echo "Could not compute the set of object files"
    exit 1
fi

#if [[ ${VERBOSE} != "" ]]; then
echo =================
echo `cat ${BUILD_FOLDER}/${COMPILESH}` ${FILTERED_OBJ} ${SOURCE_OBJ} -o ${OUTPUT}
echo =================
#fi

${BUILD_FOLDER}/${COMPILESH} ${FILTERED_OBJ} ${SOURCE_OBJ} -o ${OUTPUT}

