#!/bin/bash

## activate both for error checking
#set -e
#set -x

###################################################
# signal handlers
###################################################
trap exitscript SIGINT

#######################################################
# variables
#######################################################

# Some packages are using the version information at several places
# because we have to copy some files around
SDL_VERSION=1.2.14
SDL_MIXER_VERSION=1.2.8
SDL_TTF_VERSION=2.0.9
SDL_IMAGE_VERSION=1.2.6
CURL_VERSION=7.16.4
#//	CURL_VERSION=7.17.1
CUNIT_VERSION=2.1-0

# Use an absolute path here
TEMP_DIR=/tmp/tmp_codeblocks
TARGET_DIR=${TEMP_DIR}/UFOAIwin32BUILDenv
DOWNLOAD_DIR=${TEMP_DIR}/downloads
EXTRACT_DIR=${DOWNLOAD_DIR}/extract
MINGW_DIR=${TARGET_DIR}/MinGW
CODEBLOCKS_DIR=${TARGET_DIR}
NSIS_DIR=${TARGET_DIR}/NSIS
CB_DIR=${TARGET_DIR}/codeblocks
#//	ARCHIVE_NAME=codeblocks
LOGFILE_NAME=codeblocks.log
UN7ZIP=$(which 7za)
WGET=$(which wget)
UNZIP=$(which unzip)
TAR=$(which tar)
sfx7ZIP=$(which 7z.sfx)
environment_ver=0.2.0

if [ -x "${sfx7ZIP}" ]; then
	ARCHIVE_NAME=UfoAI_win32_build_environment_${environment_ver}.exe
else
	ARCHIVE_NAME=UfoAI_win32_build_environment_${environment_ver}.7z
fi

export LC_ALL=C



# Most packages are somewhat "fragmented" /include folders with subfolders .....
# to find include folders the "header search and move/copy algo" start a search for it ( some packages store headers inside of libs too )
# but on one gcc package the include folder inside off libs should'nt be moved
# this will exclude files or folders from this search
declare exclude_from_find_result="-not -path \"*/lib/gcc/mingw32/*\""

# just a var to store some stdout output
declare what_do_we_try

# hold the folder structure of mingw/include for the "header search and move/copy algo"
declare	include_array=(	"AL"\
			"atk"\
			"CUnit"\
			"curl"\
			"ddk"\
			"fontconfig"\
			"freetype2"\
			"gail"\
			"gdk"\
			"gdk-pixbuf"\
			"gio"\
			"GL"\
			"glib"\
			"gobject"\
			"gtk"\
			"gtksourceview"\
			"libgail-util"\
			"libpng12"\
			"libxml"\
			"ncurses"\
			"ogg"\
			"pango"\
			"SDL"\
			"sys"\
			"theora"\
			"vorbis" )










#######################################################
# functions
#######################################################

function infooutput()
{
	magentaecho "${1}"
	what_do_we_try="${1}"
}

function redecho() {
	echo -e "\033[1;31m${1}\033[0m"
}

function whiteecho() {
	echo -e "\033[1;37m${1}\033[0m"
}

function magentaecho() {
	echo -e "\033[1;35m${1}\033[0m"
}

function greenecho() {
	echo -e "\033[1;32m${1}\033[0m"
}

function failure()
{
	magentaecho "${2}"
	redecho "${1}"
	echo -e "tried to do: ${2}\nFailed: ${1}"
	echo -e "tried to do: ${2}\nFailed: ${1}" >> ${LOGFILE_NAME} 2>&1
	exit 1
}

function check_error()
{
	error=${1}
	errormessage=${2}
	if [ $error -ne 0 ]; then
		failure "Error: ${errormessage}"
	fi
}









create()
{
	mkdir -p "${TEMP_DIR}"
	rm -rf "${TARGET_DIR}"
	mkdir -p "${TARGET_DIR}"
	mkdir -p "${CODEBLOCKS_DIR}"
	mkdir -p "${MINGW_DIR}"
	mkdir -p "${DOWNLOAD_DIR}"
	mkdir -p "${CB_DIR}"
	mkdir -p "${NSIS_DIR}"
	echo $(date) > ${LOGFILE_NAME}

	start_downloads

	correctme

	echo -n $environment_ver>"${MINGW_DIR}/bin/WINenvVER"

	echo -n "Used space in ${CODEBLOCKS_DIR}: "
	echo $(du -h -c ${CODEBLOCKS_DIR} | tail -1)

	infooutput "Start to create ${ARCHIVE_NAME}"

	if [ -x "${sfx7ZIP}" ]; then
		rm -f ~/${ARCHIVE_NAME}
		echo ${UN7ZIP} a -t7z -mx=9 -sfx7z.sfx ~/${ARCHIVE_NAME} "${CODEBLOCKS_DIR}"
		${UN7ZIP} a -t7z -mx=9 -sfx7z.sfx ~/${ARCHIVE_NAME} "${CODEBLOCKS_DIR}" || failure "cant create ${ARCHIVE_NAME}" "$what_do_we_try"
	else
		rm -f ~/${ARCHIVE_NAME}
		echo ${UN7ZIP} a -t7z -mx=9 ~/${ARCHIVE_NAME} "${CODEBLOCKS_DIR}"
		${UN7ZIP} a -t7z -mx=9 ~/${ARCHIVE_NAME} "${CODEBLOCKS_DIR}" || failure "cant create ${ARCHIVE_NAME}" "$what_do_we_try"
	fi
	greenecho "codeblock created\nFINISHED"

	echo $(date) >> ${LOGFILE_NAME} 2>&1
	echo "finished creating $(du -h ~/${ARCHIVE_NAME})"
}

download()
{
	mkdir -p "${TEMP_DIR}"
	mkdir -p "${TARGET_DIR}"
	mkdir -p "${CODEBLOCKS_DIR}"
	mkdir -p "${CB_DIR}"
	mkdir -p "${NSIS_DIR}"
	mkdir -p "${MINGW_DIR}"
	mkdir -p "${DOWNLOAD_DIR}"

	start_downloads
}

function download_archive()
{
	baseurl=${1}
	filename=${2}
	targetname=${3}

	local what_do_we_try="downloading ${filename}..."
	magentaecho "$what_do_we_try"

	for ((a=0; a<=25; a++))
	do
		# if the mirror is down get another mirror
		sleep 1
		${WGET} -c --tries=1 --timeout=30 --progress=bar:force -O "${DOWNLOAD_DIR}/${targetname}" ${baseurl}${filename} && break
		if [ ${a} -eq 5 ] ;then
			failure "Could not fetch ${baseurl}${filename}" "$what_do_we_try"
		fi
	done
	greenecho "${filename} successfully downloaded"

	extract ${3}
}

function extract()
{
	infooutput "test for extraction ${1}"

	rm -rf "${EXTRACT_DIR}"
	${UN7ZIP} l "${DOWNLOAD_DIR}/${1}" || failure "${1} cant be extracted" "$what_do_we_try"
	greenecho "a 7z extractable archive"

	infooutput "extracting ${1}"

	${UN7ZIP} x -y -o"${EXTRACT_DIR}" "${DOWNLOAD_DIR}/${1}" || failure "unable to extract ${1}" "$what_do_we_try"
	greenecho "${1} extracted"

	local isTAR=$(find "${EXTRACT_DIR}" -iname "*.tar" -type f)
	echo "[$isTAR]"
	if [ -n "$isTAR" ] ;then
		infooutput "extracting $isTAR"

		${UN7ZIP} x -y -o"${EXTRACT_DIR}" "$isTAR" || failure "unable to extract $isTAR" "$what_do_we_try"
		rm -f "$isTAR"
		greenecho "$isTAR extracted"
	fi


	if [ $(echo ${1} | grep -i -c -e "^codeblocks" -e "^7zip" -e "^NSIS.zip$" -e "^rxvt.7z$" -e "^make_UfoAI_win32.7z$") -eq 0 ] ;then
		infooutput "try to find usable directories"

		for ((a=0; a<=25; a++))
		do
			local find_output=$(find "${EXTRACT_DIR}" -maxdepth ${a} -type d -iname "bin" -o -iname "contrib" -o -iname "doc" -o -iname "etc" -o -iname "home" -o -iname "include" -o -iname "info" -o -iname "lib" -o -iname "libexec" -o -iname "man" -o -iname "manifest" -o -iname "mingw32" -o -iname "share")
			if [ -n "$find_output" ] ;then
				greenecho "found usable directories"








				# header search and move/copy algo

				if [ -d "${EXTRACT_DIR}/include" ] ;then	# if there is a ${EXTRACT_DIR}/include folder
					local find_include=$(find "${EXTRACT_DIR}" -iname "include" -type d )	# search for include subfolders; /libs/glib-2.0/include
					if [ -n "$find_include" ] ;then
						local -a find_include_array
						find_include_array=(`echo -e "$find_include" | tr '\n' ' '`)
						for find_include_array_element in ${find_include_array[@]}
						do
							infooutput "found include folder $find_include_array_element"

							local find_include_subdirs=$(eval "find "$find_include_array_element" $exclude_from_find_result -iname "*.h" -type f -exec dirname {} \; | sort | uniq") # look for header files inside the include folder, but exclude things from var exclude_from_find_result
							if [ -n "$find_include_subdirs" ] ;then
								local -a find_include_subdirs_array
								find_include_subdirs_array=(`echo -e "$find_include_subdirs" | tr '\n' ' '`)

								for find_include_subdirs_array_element in ${find_include_subdirs_array[@]}
								do
									infooutput "found include subdirs $find_include_subdirs_array_element"

									if [ "$find_include_subdirs_array_element" != "${EXTRACT_DIR}/include" ] ;then	# if the header files are not inside of ..../include ; in that case we dont need to move things
										if [ -d "$find_include_subdirs_array_element" ] ;then
											if [ "$find_include_subdirs_array_element" = "$find_include_array_element" ] ;then	# if the headers are inside of /libs/glib-2.0/include ,and not /include/glib-2.0/gtk
												infooutput "cp -vR \"$find_include_subdirs_array_element/\"* \"${EXTRACT_DIR}/include\""

												cp -vR "$find_include_subdirs_array_element/"* "${EXTRACT_DIR}/include" || failure "unable to move $find_include_subdirs_array_element" "$what_do_we_try"
												rm -rf "$find_include_subdirs_array_element"
												find "${EXTRACT_DIR}" -depth -type d -empty -exec rmdir {} \;

												greenecho "\"$find_include_subdirs_array_element\" moved"
											else
												for (( i=0; i<${#include_array[@]}; i++ )) #include_array_element in ${include_array[@]} # hold dirs that must be at mingw/include
												do
										whiteecho "${i} ${include_array[${i}]}$"
													if [ $( echo $find_include_subdirs_array_element | grep -ice "${include_array[${i}]}$" ) -gt 0 ] ;then # if the subfolder inside of /somwhere/include must stay at mingw/include
														if [ $(readlink -f "$find_include_subdirs_array_element/..") != "${EXTRACT_DIR}/include" ] ;then # to be shure the directory is'nt copied on itself
															infooutput "mv -vf \"$find_include_subdirs_array_element\" \"${EXTRACT_DIR}/include\""

															mv -vf "$find_include_subdirs_array_element" "${EXTRACT_DIR}/include" || failure "unable to move $find_include_subdirs_array_element" "$what_do_we_try"
															rm -rf "$find_include_subdirs_array_element"
															find "${EXTRACT_DIR}" -depth -type d -empty -exec rmdir {} \;

															greenecho "\"$find_include_subdirs_array_element\" moved"
														fi
														break
													fi
													if [ $(((i+1))) -eq ${#include_array[@]} ] ;then	# if the folder is not part of mingw/include like /include/cairo then move all things from that folder at mingw/include
														infooutput "mv -vf \"$find_include_subdirs_array_element/\"* \"${EXTRACT_DIR}/include\""

														mv -vf "$find_include_subdirs_array_element/"* "${EXTRACT_DIR}/include" || failure "unable to move $find_include_subdirs_array_element" "$what_do_we_try"
														rm -rf "$find_include_subdirs_array_element"
														find "${EXTRACT_DIR}" -depth -type d -empty -exec rmdir {} \;

														greenecho "\"$find_include_subdirs_array_element/\"* moved"
													fi
												done
											fi
										fi
									else
										break
									fi
								done
							fi
						done
					fi
					find "${EXTRACT_DIR}" -depth -type d -empty -exec rmdir {} \;
				fi
				# END header search and move/copy algo






				infooutput "copy usable directories"

				local -a array
				array=(`echo -e "$find_output" | tr '\n' ' '`)

				for s in ${array[@]}
				do
					if [ -d "$s" ] ;then
						cp -v -R "$s" "${MINGW_DIR}" || failure "unable to copy $s" "$what_do_we_try"
						greenecho "$s copied"
					fi
				done
				break
			fi
			if [ ${a} -eq 25 ] ;then
				failure "cant find usable directories" "$what_do_we_try"
			fi
		done
	else
		infooutput "found codeblocks or 7zip or NSIS or rxvt archive"

		if [ $(echo ${1} | grep -i -c -e "^codeblocks") -gt 0 ] ;then
			infooutput "try to copy codeblocks files"

			cp -v -R "${EXTRACT_DIR}/"* "${CB_DIR}" || failure "unable to copy codeblocks files" "$what_do_we_try"
			greenecho "codeblocks copied"
		fi
		if [ $(echo ${1} | grep -i -c -e "^7zip") -gt 0 ] ;then
			infooutput "try to copy 7zip files"

			if [ -e "${EXTRACT_DIR}/7za.exe" ] ;then
				cp -v -R "${EXTRACT_DIR}/7za.exe" "${MINGW_DIR}/bin" || failure "unable to copy 7za.exe" "$what_do_we_try"
				greenecho "7za.exe copied"
			else
				failure "unable to find 7za.exe" "$what_do_we_try"
			fi
#			if [ -e "${EXTRACT_DIR}/7zS.sfx" ] ;then
#				cp -v -R "${EXTRACT_DIR}/7zS.sfx" "${MINGW_DIR}/bin" || failure "unable to copy 7zS.sfx" "$what_do_we_try"
#				greenecho "7zS.sfx copied"
#			else
#				failure "unable to find 7zS.sfx" "$what_do_we_try"
#			fi
		fi
		if [ $(echo ${1} | grep -i -c -e "^NSIS.zip$") -gt 0 ] ;then
			infooutput "try to copy NSIS files"

			local makenspath=$(find "${EXTRACT_DIR}" -iname "makensisw.exe" -type f | sed 's/makensisw.exe//gI')
			if [ -n "$makenspath" ] ;then
				cp -v -R "$makenspath"* "${NSIS_DIR}" || failure "unable to copy NSIS files" "$what_do_we_try"
				greenecho "NSIS copied"
			else
				failure "unable to find makensisw.exe" "$what_do_we_try"
			fi
		fi
		if [ $(echo ${1} | grep -i -c -e "^rxvt.7z$") -gt 0 ] ;then
			infooutput "try to copy rxvt files"

			cp -v -R "${EXTRACT_DIR}/"* "${MINGW_DIR}" || failure "unable to copy rxvt files" "$what_do_we_try"
			greenecho "rxvt copied"
		fi
		if [ $(echo ${1} | grep -i -c -e "^make_UfoAI_win32.7z$") -gt 0 ] ;then
			infooutput "try to copy make_UfoAI_win32 files"

			cp -v -R "${EXTRACT_DIR}/"* "${CODEBLOCKS_DIR}" || failure "unable to copy rxvt make_UfoAI_win32" "$what_do_we_try"
			greenecho "make_UfoAI_win32 copied"
		fi
	fi
	sleep 1
#	rm -rf "${EXTRACT_DIR}"
}

function correctme()
{
	#here we do some correction for all possible things
	infooutput "doing some corrections if necessary"

	#glib-2.0
#	if [ -f "${MINGW_DIR}/include/glib-2.0/glib.h" ] ;then
#		infooutput "try to move header for glib-2.0"
#
#		mv -f -v "${MINGW_DIR}/include/glib-2.0/"* "${MINGW_DIR}/include" || failure "unable to move headers" "$what_do_we_try"
#		greenecho "headers moved"
#		rm -rf "${MINGW_DIR}/include/glib-2.0"
#	fi
}

function start_downloads()
{
	download_archive http://downloads.sourceforge.net/mingw/ binutils-2.20-1-mingw32-bin.tar.gz binutils.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ mingwrt-3.17-mingw32-dev.tar.gz mingwrt.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ w32api-3.14-mingw32-dev.tar.gz w32api.tar.gz
#	download_archive http://downloads.sourceforge.net/mingw/ gdb-7.0-2-mingw32-bin.tar.gz gdb.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ gdb-7.0.50.20100202-mingw32-bin.tar.gz gdb.tar.gz
#	download_archive http://downloads.sourceforge.net/mingw/ mingw32-make-3.81-20080326-3.tar.gz make.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ make-3.81-20090914-mingw32-bin.tar.gz make.tar.gz
#	download_archive http://downloads.sourceforge.net/mingw/ bash-3.1-MSYS-1.0.11-1.tar.bz2 msys-bash.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ bash-3.1.17-2-msys-1.0.11-bin.tar.lzma msys-bash.tar.bz2
#	download_archive http://downloads.sourceforge.net/mingw/ bzip2-1.0.3-MSYS-1.0.11-1.tar.bz2 msys-bzip2.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ bzip2-1.0.5-2-mingw32-bin.tar.gz msys-bzip2.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ libbz2-1.0.5-2-mingw32-dll-2.tar.gz libbz2-bzip2.tar.gz
#	download_archive http://downloads.sourceforge.net/mingw/ tar-1.19.90-MSYS-1.0.11-2-bin.tar.gz msys-tar.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ tar-1.22-1-msys-1.0.11-bin.tar.lzma msys-tar.tar.lzma
#	download_archive http://downloads.sourceforge.net/mingw/ coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2 msys-coreutils.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ coreutils-5.97-2-msys-1.0.11-bin.tar.lzma msys-coreutils.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ coreutils-5.97-2-msys-1.0.11-ext.tar.lzma msys-coreutils-ext.tar.lzma
#	download_archive http://downloads.sourceforge.net/mingw/ findutils-4.3.0-MSYS-1.0.11-3-bin.tar.gz msys-findutils.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ findutils-4.4.2-1-msys-1.0.11-bin.tar.lzma msys-findutils.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ locate-4.4.2-1-msys-1.0.11-bin.tar.lzma msys-locate.tar.lzma
#	download_archive http://downloads.sourceforge.net/mingw/ MSYS-1.0.11-20090120-dll.tar.gz msys.tar.gz
#	download_archive http://downloads.sourceforge.net/mingw/ msysCORE-1.0.11-20080826.tar.gz msys-core.tar.gz
	download_archive http://downloads.sourceforge.net/mingw/ msysCORE-1.0.13-2-msys-1.0.13-bin.tar.lzma msys-core.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ m4-1.4.13-1-msys-1.0.11-bin.tar.lzma m4.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ sed-4.2.1-1-msys-1.0.11-bin.tar.lzma sed.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ patch-2.5.9-1-msys-1.0.11-bin.tar.lzma patch.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ diffutils-2.8.7.20071206cvs-2-msys-1.0.11-bin.tar.lzma diffutils.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ grep-2.5.4-1-msys-1.0.11-bin.tar.lzma grep.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ make-3.81-2-msys-1.0.11-bin.tar.lzma make.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ less-436-1-msys-1.0.11-bin.tar.lzma less.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ texinfo-4.13a-1-msys-1.0.11-bin.tar.lzma texinfo.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ gzip-1.3.12-1-msys-1.0.11-bin.tar.lzma gzip.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ file-5.03-1-msys-1.0.11-bin.tar.lzma file.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ mktemp-1.6-1-msys-1.0.11-bin.tar.lzma mktemp.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ perl-5.6.1_2-1-msys-1.0.11-bin.tar.lzma perl.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ gawk-3.1.7-1-msys-1.0.11-bin.tar.lzma gawk.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libiconv-1.13.1-1-msys-1.0.11-dev.tar.lzma libiconv-dev.tar.lzma
	download_archive http://downloads.sourceforge.net/mingw/ libiconv-1.13.1-1-msys-1.0.11-bin.tar.lzma libiconv-bin.tar.lzma
#	download_archive http://downloads.sourceforge.net/mingw/ msys-autoconf-2.59.tar.bz2 msys-autoconf.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ autoconf-2.61-MSYS-1.0.11-1.tar.bz2 msys-autoconf.tar.bz2
#	download_archive http://downloads.sourceforge.net/mingw/ msys-automake-1.8.2.tar.bz2 msys-automake.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ automake-1.10-MSYS-1.0.11-1.tar.bz2 msys-automake.tar.bz2
#	download_archive http://downloads.sourceforge.net/mingw/ msys-libtool-1.5.tar.bz2 msys-libtool.tar.bz2
	download_archive http://downloads.sourceforge.net/mingw/ libtool1.5-1.5.25a-20070701-MSYS-1.0.11-1.tar.bz2 msys-libtool.tar.bz2
#gnome	download_archive http://downloads.sourceforge.net/mingw/ gettext-0.17-1-mingw32-dev.tar.lzma gettext.lzma

# TODO mingw64 support
#	download_archive http://downloads.sourceforge.net/mingw-w64 mingw-w64-bin_x86_64-mingw_20100123_sezero.zip

	download_archive http://downloads.sourceforge.net/tdm-gcc/ gcc-4.4.1-tdm-2-core.tar.gz gcc.tar.gz
	download_archive http://downloads.sourceforge.net/tdm-gcc/ gcc-4.4.1-tdm-2-g++.tar.gz g++.tar.gz

	download_archive http://downloads.sourceforge.net/gnuwin32/ freetype-2.3.5-1-lib.zip freetype.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ jpeg-6b-4-lib.zip libjpeg.zip
#mingw	download_archive http://downloads.sourceforge.net/gnuwin32/ libiconv-1.9.2-1-lib.zip libiconv.zip
#mingw	download_archive http://downloads.sourceforge.net/gnuwin32/ libiconv-1.9.2-1-bin.zip libiconv-bin.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ libintl-0.14.4-lib.zip libintl.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ libintl-0.14.4-bin.zip libintl-bin.zip
#	download_archive http://downloads.sourceforge.net/gnuwin32/ libpng-1.2.35-lib.zip libpng.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ libpng-1.2.37-lib.zip libpng.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ pdcurses-2.6-lib.zip libpdcurses.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ tiff-3.8.2-1-lib.zip libtiff.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ wget-1.11.4-1-bin.zip wget.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ wget-1.11.4-1-dep.zip wget-dep.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ zlib-1.2.3-lib.zip zlib.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ unzip-5.51-1-bin.zip unzip.zip
#	download_archive http://downloads.sourceforge.net/gnuwin32/ openssl-0.9.8h-1-bin.zip openssl.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ regex-2.7-bin.zip regex.zip
	download_archive http://downloads.sourceforge.net/gnuwin32/ pcre-7.0-bin.zip pcre.zip
#	download_archive http://downloads.sourceforge.net/gnuwin32/ grep-2.5.4-bin.zip grep.zip

	download_archive http://downloads.sourceforge.net/cunit/ CUnit-${CUNIT_VERSION}-winlib.zip cunit.zip

	download_archive http://downloads.sourceforge.net/sevenzip/ 7za910.zip 7zip.zip

	download_archive http://curl.oslevel.de/download/ libcurl-${CURL_VERSION}-win32-nossl.zip libcurl.zip

	download_archive http://www.libsdl.org/release/ SDL-devel-${SDL_VERSION}-mingw32.tar.gz sdl.tar.gz
	download_archive http://www.libsdl.org/projects/SDL_ttf/release/ SDL_ttf-devel-${SDL_TTF_VERSION}-VC8.zip sdl_ttf.zip
	download_archive http://www.libsdl.org/projects/SDL_mixer/release/ SDL_mixer-devel-${SDL_MIXER_VERSION}-VC8.zip sdl_mixer.zip
	download_archive http://www.libsdl.org/projects/SDL_image/release/ SDL_image-devel-${SDL_IMAGE_VERSION}-VC8.zip sdl_image.zip

	download_archive http://oss.netfarm.it/mplayer/pkgs/ libvorbis-mingw32-1.3.1-gcc42.tar.bz2 libvorbis.tar.bz2
	download_archive http://oss.netfarm.it/mplayer/pkgs/ libtheora-mingw32-1.1.1-gcc42.tar.bz2 libtheora.tar.bz2
	download_archive http://oss.netfarm.it/mplayer/pkgs/ xvidcore-mingw32-1.2.2-vaq-gcc42.tar.bz2 xvidcore.tar.bz2
	download_archive http://oss.netfarm.it/mplayer/pkgs/ libogg-mingw32-1.2.0-gcc42.tar.bz2 libogg.tar.bz2

#	download_archive http://downloads.xiph.org/releases/ogg/ libogg-1.1.3.tar.gz libogg.tar.gz
#	download_archive http://downloads.xiph.org/releases/ogg/ libvorbis-1.2.0.tar.gz libvorbis.tar.gz

	download_archive http://download.berlios.de/codeblocks/ wxmsw28u_gcc_cb_wx2810.7z codeblocks_gcc.7z
	download_archive http://download.berlios.de/codeblocks/ mingwm10_gcc421.7z codeblocks_mingw.7z
	download_archive http://download.berlios.de/codeblocks/ CB_20100116_rev6088_win32.7z codeblocks.7z

#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.22/ glib_2.22.2-1_win32.zip glib.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.22/ glib_2.22.3-1_win32.zip glib.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.22/ glib-dev_2.22.2-1_win32.zip glib-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.22/ glib-dev_2.22.3-1_win32.zip glib-dev.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.18/ gtk+-dev_2.18.3-1_win32.zip gtk+-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.18/ gtk+-dev_2.18.5-1_win32.zip gtk+-dev.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/gtksourceview/2.8/ gtksourceview-dev-2.8.1.zip gtksourceview-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/gtksourceview/2.9/ gtksourceview-dev-2.9.3.zip gtksourceview-dev.zip
#	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.26/ pango-dev_1.26.0-1_win32.zip pango-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.26/ pango-dev_1.26.1-1_win32.zip pango-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/atk/1.28/ atk-dev_1.28.0-1_win32.zip atk-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ cairo-dev_1.8.8-3_win32.zip cairo-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ gettext-runtime-0.17-1.zip gettext-runtime.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ gettext-tools-0.17.zip gettext-tools.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ gettext-runtime-dev-0.17-1.zip gettext-runtime-dev.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ pkg-config_0.23-3_win32.zip pkg-config.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ libxml2-dev_2.7.4-1_win32.zip libxml2.zip
	download_archive http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/ fontconfig-dev_2.8.0-1_win32.zip fontconfig.zip

	download_archive http://mattn.ninex.info/download/ gtkglext-1.2.zip gtkglext-dev.zip
	download_archive http://mattn.ninex.info/download/ openal.zip libopenal-dev.zip

#	download_archive http://subversion.tigris.org/files/documents/15/45600/ svn-win32-1.6.1.zip svn.zip
	download_archive http://subversion.tigris.org/files/documents/15/46880/ svn-win32-1.6.6.zip svn.zip

	download_archive http://www.libsdl.org/extras/win32/common/ directx-devel.tar.gz directx.tar.gz

	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/ rxvt.7z rxvt.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/ make_UfoAI_win32.7z make_UfoAI_win32.7z
	download_archive http://www.meduniwien.ac.at/user/michael.zellhofer/ufoai/ 7z_sfx.7z 7z_sfx.7z

	download_archive http://downloads.sourceforge.net/nsis/ nsis-2.46.zip NSIS.zip
}

function exitscript()
{
	exit 2
}



#######################################################
# don't change anything below here
#######################################################







if [ ! -x "${UN7ZIP}" ]; then
	echo "you need 7zip installed to run this script"
	exit 1
fi

if [ ! -x "${WGET}" ]; then
	echo "you need wget installed to run this script"
	exit 1
fi






ACTION=${1:-help}

if [ "$ACTION" = "create" ]; then
	create
elif [ "$ACTION" = "download" ]; then
	download
elif [ "$ACTION" = "help" ]; then
	echo "Valid parameters are:"
	echo " - create   : creates the ${ARCHIVE_NAME} with the"
	echo "              latest versions of the needed programs and libs"
	echo " - download : only performs the downloading"
	echo " - clean    : removes the downloads and the temp dirs - but not"
	echo "              the created archive file"
	echo " - help     : prints this help message"
	echo " - upload   : uploads the created archive - ./ssh/config must be"
	echo "              set up for this to work"
elif [ "$ACTION" = "upload" ]; then
	if [ ! -e "${ARCHIVE_NAME}" ]; then
		echo "${ARCHIVE_NAME} doesn't exist. Hit any key to create it."
		read
		create
	fi
	scp ${ARCHIVE_NAME} ufo:~/public_html/mattn/download
elif [ "$ACTION" = "clean" ]; then
	rm -rf "${TEMP_DIR}"
	echo "clean finished"
else
	echo "unknown action given"
	exit 1
fi





# changelog
#	0.1.0 UfoAI devs
#	0.2.0 Muton (based on codeblocks.sh rev. 28557)
#		The script:
#		will now use 7za only for extraction
#		add everything needed to build UfoAI incl. NSIS and keep things separated; codeblock is codeblock, mingw is mingw
#		added my script make_UfoAI_win32 too
#		changed the way the script calls functions; I download and extract package by package
#		made a lot of error checking
#		header search and move/copy algo to move header files to the correct place (include_array)
#		The package is now called UfoAI_win32_build_environment and the folder UFOAIwin32BUILDenv
#		new
#			added variable environment_ver, exclude_from_find_result, what_do_we_try, EXTRACT_DIR, NSIS_DIR, CB_DIR, sfx7ZIP
#			added array include_array
#			added function infooutput(), redecho(), whiteecho(), magentaecho(), greenecho(), extract(), correctme()
#			added changelog
#			store a txt file at "${MINGW_DIR}/bin/WINenvVER" containing the version of the last build
#		changed
#			changed variable ARCHIVE_NAME, UN7ZIP, TARGET_DIR
#			changed function failure(), start_downloads(), create(), download(), download_archive(),
#			moved global vars topside
#			removed function extract_ogg(), extract_openal(), extract_gtk(), extract_tools(), extract_sdl(), extract_libcurl(), extract_cunit(), extract_libs(), extract_mingw(), extract_codeblocks(), extract_archive_gz(), extract_archive_bz2(), extract_archive_zip(), extract_archive_7z(), extract_archive_tar7z(),
#		update package
#			svn
#			pango
#			gtksourceview
#			gtk+-dev
#			glib-dev
#			glib
#			libpng
#			libiconv
#			gettext
#			libtool
#			automake
#			autoconf
#			msysCORE
#			findutils
#			coreutils
#			tar
#			bzip2
#			bash
#			make
#	      Muton (2010.04.17)
#		update package
#			libvorbis
#			xvidcore
#			libogg
