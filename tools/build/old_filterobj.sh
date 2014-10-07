###################
#
#    DEPRECATED
#
###################

BUILD_FOLDER=$1
COMPILESH=$2
SOURCE_OBJ=$3
OUTPUT=$4
VERBOSE=$5


# This script compiles a program using the compilation script 
# ${BUILD_FOLDER}/${COMPILESH}, by passing it as argument an 
# appropriate list of *.o files. This list is obtained by taking
# all of the files ${BUILD_FOLDER}/*.o  and then filtering all
# of the $foo.o files such that $foo.cpp exists in the current
# folder and $foo.o is not the same as ${SOURCE_OBJ}.o.


ALL_OBJ=`ls ${BUILD_FOLDER}/*.o`
ALL_PROGS=`ls *.cpp`

EXCLUDE_OBJ=
for SRC in ${ALL_PROGS}; do 
  OBJ=${SRC%.*}.o
  EXCLUDE_OBJ="${EXCLUDE_OBJ} ${BUILD_FOLDER}/${OBJ}"
done

FILTERED_OBJ=
if [[ "${EXCLUDE_OBJ}" == "" ]]; then
  FILTERED_OBJ="${ALL_OBJ}"
else
  for OBJ in ${ALL_OBJ}; do 
    SKIP=0
    for EXC in ${EXCLUDE_OBJ}; do 
      if [[ "${OBJ}" == "${EXC}" ]]; then
        SKIP=1
        break
      fi
    done
    if [[ ${SKIP} == 0 ]]; then
      FILTERED_OBJ="${FILTERED_OBJ} ${OBJ}"
    fi
  done
fi

if [[ ${VERBOSE} != "" ]]; then
echo =================
echo `cat ${BUILD_FOLDER}/${COMPILESH}` ${FILTERED_OBJ} ${SOURCE_OBJ} -o ${OUTPUT}
echo =================
fi
${BUILD_FOLDER}/${COMPILESH} ${FILTERED_OBJ} ${SOURCE_OBJ} -o ${OUTPUT}


#echo ALL_OBJ=${ALL_OBJ}
# echo EXCLUDE_OBJ=${EXCLUDE_OBJ}
# echo FILTERED_OBJ=${FILTERED_OBJ}

# ALL_OBJ="a b c dd e f g"
# EXCLUDE_OBJ="b d e"
# FILTERED_OBJ=


