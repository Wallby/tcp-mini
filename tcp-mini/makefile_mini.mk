MAKEFILE_MINI_VERSION:=0.1.2

# usage..
# make #< default target makes all not ignored binaries
# make test #< make all test(s) if any and run them
# make release
# # ^
# # 1. make all not ignored binaries + make test(s) if any
# # 2. run test(s) if any and only continue if all test(s) passed
# # 3. make any releasetype(s) specified
# make clean #< clean all binaryparts and binaries
#
# rules..
# .. for paths use / only never \, if required (only if required)..
#    .. makefile-mini will automatically replace / with \
#
# terminology..
# .. project <- this Makefile
# .. resource <- e.g. for mm_add_library a library is a resource
# .. binarypartsource <- e.g. .c for .o
# .. binarypart <- e.g. .o for .lib/.a/.exe/<no extension>
# .. binarysource <- e.g. .glsl for .spv
# .. binary <- e.g. for $(call mm_add_library,staticlibrarytest,<..>)..
#    .. libstaticlibrarytest.<lib/a> is a binary
# .. library <- static|shared
# .. executable <- <no extension>/.exe, optionally.. portable (.AppImage/.exe)
# .. installer <- <.deb/.snap>/.msi
#
# projectname.. e.g. for myproject/makefile_mini.mk included by..
# .. myproject/Makefile.. myproject
#
# resourcename..
# .. may not contain . (see "external project address")
# .. may contain / (\ will be replaced by / for consistency)*
# .. per resourcetype every resourcename must be unique**
# ^
# * e.g. test/test.spv.h..
#   char $(notdir $(<.name>))_spv_h[] = { <...> };
#   v
#   char test_spv_h[] = { <...> };
#   OR
#   char $(subst /,_,$(dir $(<.name>)))$(notdir $(<.name>))_spv_h[] = {..
#   .. <...> };
#   v
#   char test_test_spv_h[] = { <...> };
#   ^
#   .makefile-mini/test/test.spv.h would still result in the above output as..
#   .. .makefile-mini is ignored here
# ** resourcename is always specified by type (e.g...
#    .. <mm_add_library_parameters_t>.libraries)
#    ^
#    <mm_start_parameters_t>.ignoredbinaries and..
#    .. <mm_stop_parameters>.ifRelease.<ignoredbinaries/if*.ignoredbinaries>..
#    .. are specified as regular expression, resourcename is not allowed here
#    ^
#    exceptions..
#    .. shader and shaderlibrary as both use .spv and .spv.h
#
# external project address..
# .. <projectname>:<resourcename> <- resourcename may not contain .
# .. <projectname>:<filepath> <- filepath must contain .
# ^
# if * contains a . in <projectname>:*.. * is considered a filepath
# otherwise.. * is considered a resourcename
# both resourcename and filepath may contain one or more / (e.g...
# .. test:test/test)
# if both external project address and binarypartsource is allowed..
# .. <binarypartsource> <- may not contain :
# .. <projectname>:* <- must contain :
# leave <projectname> empty for current project (e.g. :test for resource test)
# ^
# only files that don't use . ever are..
# .. Makefile (shouldn't be included as .mk is for inclusion only)
# .. linux executable (shouldn't be included as is platform specific thus..
#    .. use resourcename)

# # NOTE: $(1) == * (non cli) #< make may parse argument (e.g. for use in..
# #       .. $(subst <..>))
# #       $(2) == * (cli) #< make does not parse argument (e.g. $@)
# mm_cli_*

#MM_SAFETY:=


#******************************************************************************

# NOTE: https://stackoverflow.com/a/47927343/4825512
MM_EMPTY:=
define MM_NEWLINE:=

$(MM_EMPTY)
endef
MM_COMMA:=,
MM_SPACE:=$(MM_EMPTY) $(MM_EMPTY)
MM_PERCENT:=%

ifndef OS #< linux
MM_OS:=linux

MM_FOLDER_SEPARATOR:=/
MM_STATICLIBRARY_EXTENSION:=.a
MM_SHAREDLIBRARY_EXTENSION:=.so
MM_EXECUTABLE_EXTENSION:=
MM_SCRIPT_EXTENSION:=
MM_PORTABLEEXECUTABLE_EXTENSION:=.AppImage
MM_RELEASEINSTALLER_EXTENSIONS:=.deb .snap


MM_CLI_DEV_NULL:=/dev/null
else ifeq ($(OS), Windows_NT) #< windows
MM_OS:=windows

MM_FOLDER_SEPARATOR:=\$(MM_EMPTY)
MM_STATICLIBRARY_EXTENSION:=.lib
MM_SHAREDLIBRARY_EXTENSION:=.dll
MM_EXECUTABLE_EXTENSION:=.exe
MM_SCRIPT_EXTENSION:=.bat
MM_PORTABLEEXECUTABLE_EXTENSION:=.exe
MM_RELEASEINSTALLER_EXTENSIONS:=.msi

MM_CLI_DEV_NULL:=NUL
else
$(error os not supported)
endif
MM_EXECUTABLE_EXTENSION_OR_DOT:=$(if $(MM_EXECUTABLE_EXTENSION),$(MM_EXECUTABLE_EXTENSION),.)

ifndef OS #< linux
# NOTE: $(1) = non cli (see windows version of mm_cli_mkdir)
mm_cli_mkdir=mkdir -p $(1)
# TODO: not tested
# NOTE: $(1) == non cli (see windows version of mm_cli_rmdir)
mm_cli_rmdir=rm -d -f $(1)
# NOTE: ^
#       not using rmdir because that will error if folder doesn't exist

mm_cli_rm=rm -f $(1)

# NOTE: $(2) == non cli (see windows version of mm_cli_zip)
mm_cli_zip=zip -r9 $(1) $(2)

# NOTE: $(1) == one or element(s) of format <pattern>/<replacement>
define mm_cli_not_sed=
$(eval mm_cli_not_sed_a:=$(subst ",\",$(1)))
$(eval mm_cli_not_sed_b:=$(firstword $(mm_cli_not_sed_a)))
$(eval mm_cli_not_sed_c:=$(wordlist 2,$(words $(mm_cli_not_sed_a))))
"s/$(mm_cli_not_sed_b)/g$(patsubst %,;s/%/g,$(mm_cli_not_sed_c))"
endef
# NOTE: $(1) == one or element(s) of format <pattern>/<replacement>
#       $(2) == input
define mm_cli_sed=
$(2) | sed -e $(call mm_cli_not_sed,$(1))
endef
# NOTE: ^
#       sed "s/<..>/g;s/<..>/g" <..> > <..>
# NOTE: ^
#       " in <pattern>/<replacement> is replaced with \"
# NOTE: $(1) == one or element(s) of format <pattern>/<replacement>
#       $(2) == input filename
mm_cli_sed2=sed $(call mm_cli_not_sed,$(1)) $(2)
# NOTE: $(1) == one or element(s) of format <pattern>/<replacement>
#       $(2) == input filename
#       $(3) == output filename
mm_cli_sed3=$(call mm_cli_sed2,$(1),$(2)) > $(3)
else #< windows
# NOTE: $(1) == non cli
# NOTE: mkdir outputs "The syntax of the command is incorrect." if any /..
#       .. (only \ allowed)
mm_cli_mkdir=if not exist $(1) mkdir $(subst /,\,$(1))
# NOTE: $(1) == non cli
# NOTE: rmdir outputs "Invalid switch - \"<...>\"" if any / (only \ allowed)
mm_cli_rmdir=if exist $(1) rmdir /S /Q $(subst /,\,$(1))

# NOTE: del outputs "Invalid switch" if any forward / is used"
mm_cli_rm=if exist $(1) del $(subst /,\,$(1))

# NOTE: $(2) == non cli
mm_cli_zip=powershell "Compress-Archive $(subst $(MM_SPACE),$(MM_COMMA),$(strip $(2))) $(1)"
# NOTE: ^
#       cannot find documentation on omitting -Command for powershell but..
#       .. seems to work
#       ^
#       https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_powershell_exe?view=powershell-5.1#-command
# NOTE: ^
#       "use commas to separate the paths",..
#       .. https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.archive/compress-archive?view=powershell-7.3#-path
# NOTE: ^
#       strip in $(strip $(2)) to avoid multiple consecutive commas

# NOTE: $(1) == one or more element(s) of format <pattern>/<replacement>
define mm_cli_not_sed=
$(eval mm_cli_not_sed_a:=$(subst ",`\",$(1)))
$(subst /,\"$(MM_COMMA)\",$(patsubst %,-Replace \"%\",$(mm_cli_not_sed_a)))
endef
# NOTE: $(1) == one or more element(s) of format <pattern>/<replacement>
#       $(2) == input
mm_cli_sed=powershell "\"$(2)\" $(call mm_cli_not_sed,$(1))"
# NOTE: ^
#       powershell "Get-Content <..> -Replace \"<..>\",\"<..>\" -Replace..
#       .. \"<..>\",\"<..>\" | Set-Content <..>"
# NOTE: ^
#       " in <pattern>/<replacement> is replaced with `"
# NOTE: $(1) == one or more element(s) of format <pattern>/<replacement>
#       $(2) == input filename
mm_cli_sed2=powershell "Get-Content $(2) $(call mm_cli_not_sed,$(1))"
# NOTE: $(1) == one or more element(s) of format <pattern>/<replacement>
#       $(2) == input filename
#       $(3) == output filename
mm_cli_sed3=powershell "Get-Content $(2) $(call mm_cli_not_sed,$(1)) | Set-Content $(3)"
endif

ifndef OS #< linux
# NOTE: $(1) == filename except extension (non cli)
#       $(2) == inputfile (cli)
#       $(3) == outputfile (cli)
define mm_cli_hfile_from_file=
$(strip\
	$(eval $(0)_a:=$(shell echo -n $(1) | tr -c a-zA-Z0-9 _))\
	$(eval $(0)_b:=$(shell echo -n $($(0)_a) | tr a-z A-Z))\
	echo "#ifndef $($(0)_b)_H" > $(3);\
	echo "#define $($(0)_b)_H" >> $(3);\
	echo >> $(3); echo >> $(3);\
	echo -n "char $($(0)_a)_h[] = { " >> $(3);\
	od -An -v -td1 $(2) | tr -s " " | tr -d "\n" | sed -z -e "s/ /, /g;s/^, //;s/, $$$$//" >> $(3);\
	echo " };" >> $(3);\
	echo >> $(3);\
	echo "#endif" >> $(3)\
)
endef
# NOTE: ^
#       https://unix.stackexchange.com/a/758531
else #< windows
# NOTE: $(1) == filename except extension (non cli)
#       $(2) == inputfile (cli)
#       $(3) == outputfile (cli)
define mm_cli_hfile_from_file=
$(strip\
	$(eval $(0)_a:=$(shell powershell.exe "\"$(1)\" -Replace \"[^a-zA-Z0-9]\",\"_\""))\
	$(eval $(0)_b:=$(shell powershell.exe ""\"$($(0)_a)\".ToUpper()"))\
	powershell.exe "$$$$a=(Get-Content -Encoding Byte -Raw $(2) | Out-String).Replace(\"`r`n\",\", \").TrimEnd(\", \"); Set-Content -NoNewline \"#ifndef $($(0)_b)_H`n#define $($(0)_b)_H`n`n`nchar $($(0)_a)_h[] = { $$$$a };`n`n#endif`n\"" $(3)\
)
endef
endif

# NOTE: $(1) == a
#       $(2) == b
# NOTE: if a == b.. returns 1
#       otherwise.. returns 0
# NOTE: not the same as $(if $(filter $(1),$(2),,) as mm_equals also..
#       .. returns 1 if both $(1) and $(2) are both empty
define mm_equals=
$(eval $(0)_a:=$(strip $(1)))
$(eval $(0)_b:=$(strip $(2)))
$(eval $(0)_bAreBothEmpty:=0)
$(if $($(0)_a),,$(if $($(0)_b),,$(eval $(0)_bAreBothEmpty:=1)))
$(if $(filter 1,$($(0)_bAreBothEmpty)),\
	1,\
	$(if $(filter $($(0)_a),$($(0)_b)),1,0)\
)
endef

# NOTE: switch(<$1>)
#       {
#       case <$(word 1,$(2))>:
#         <$(word 1,$(3))>
#         break;
#       case <$(word 2,$(2)>:
#         <$(word 2,$(3))>
#         break;
#       //...
#       };
define mm_switch=
$(if $(filter 1,$(call mm_equals,$(1),$(firstword $(2)))),\
	$(firstword $(3)),\
	$(eval $(0)_a:=$(words $(2)))\
	$(if $(filter 1,$($(0)_a)),,\
		$(eval $(0)_b:=$(wordlist 2,$($(0)_a),$(2)))\
		$(eval $(0)_c:=$(wordlist 2,$($(0)_a),$(3)))\
		$(call mm_switch,$(1),$($(0)_b),$($(0)_c))\
	)\
)
endef

# NOTE: $(1) == elements variablename
mm_add_or_append_one_element=$(eval $(1)+=$(1).$(words $($(1))))

# NOTE: $(1) == pattern(s)
#       $(2) == text
# NOTE: like $(filter <..>) but using regular expression pattern
define mm_filter_using_patterns=
$(strip\
	$(eval $(0)_a:=$(firstword $(2)))\
	$(eval $(0)_b:=$(wordlist 2,$(words $(2)),$(2)))\
	$(eval $(0)_c:=$(firstword $(1)))\
	$(eval $(0)_d:=$(wordlist 2,$(words $(1)),$(1)))\
	$(if $(OS),\
	$(shell powershell "\"$($(0)_a)\"$(patsubst %,$(MM_COMMA)\"%\",$($(0)_b)) | Select-String -Pattern \"$($(0)_c)\"$(patsubst %,$(MM_COMMA)\"%\",$($(0)_d))"),\
	$(shell (echo "$($(0)_a)"$(patsubst %,; echo "%",$($(0)_b))) | grep -i -E "$($(0)_c)$(addprefix |,$($(0)_d))")\
	)\
)
endef
# NOTE: powershell "\"<..>\",\"<..>\" | Select-String -Pattern \"<..>\",\"<..>\""
#                   ^
#       no ^ required as surrounding with "" seems to escape all characters..
#       .. between ""
#       cmd.exe escaping would be powershell """"<..>""","""<...>""" |..
#       .. Select-String -Pattern """<..>""","""<..>""""
#       ^
#       https://stackoverflow.com/a/15262019
#       ^
#       powershell seems to also escape quotes if preceeded by \
# NOTE: (echo "<..>"; echo "<..>") | grep -i -E "<..>|<..>"
#                                         ^
#                                -i for consistency between windows and linux?

# NOTE: $(1) == pattern(s)
#       $(2) == text
# NOTE: like $(filter-out <..>) but using regular expression pattern
mm_filter_out_using_patterns=$(filter-out $(call mm_filter_using_patterns,$(1),$(2)),$(2))

ifndef OS #< linux
# NOTE: list files in current folder and deeper folder(s) if any recursively
mm_get_path_to_file_per_file:=$(shell find -type f)
else #< windows
# NOTE: list files in current folder and deeper folder(s) if any recursively
mm_get_path_to_file_per_file:=$(subst ",,$(shell forfiles /s /c "cmd /c if @ISDIR==FALSE echo @RELPATH"))
# NOTE: ^
#       dir /S /B /A-D
#       ^
#       outputs absolute paths
endif

#******************************************************************************
#                                    checks
#******************************************************************************

# NOTE: $(1) == functionname
#       $(2) == variablename
mm_check_if_defined=$(if $(filter undefined,$(origin $(2))),$(error $(2) is not defined in $(1)),)

# NOTE: $(1) == functionname
#       $(2) == value pattern
#       $(3) == values variablename
define mm_check_if_valid_values=
$(eval $(0)_a:=$(filter-out $(2),$($(3))))
$(if $($(0)_a),$(error $(3) contains invalid element(s) $($(0)_a) in $(1)),)
endef

# NOTE: $(1) == functionname
#       $(2) == value pattern
#       $(3) == value variablename
define mm_check_if_valid_value=
$(eval $(0)_a:=$(filter-out $(2),$($(3))))
$(if $($(0)_a),$(error $(3) contains invalid value $($(0)_a) in $(1)),)
endef

#******************************************************************************
#                                    start
#******************************************************************************

$(shell $(call mm_cli_mkdir,.makefile-mini))

MM_PROJECTNAME:=$(lastword $(subst /, ,$(abspath .)))
# NOTE: .makefile-mini/<binarypart>
MM_BINARYPARTS:=
# NOTE: .makefile-mini/<binarypathfolderpathpart>$(notdir <binarypart>)
#       ^
#       thus $(dir <binarypath>)
MM_FOLDERPATHPART_PER_BINARYPART:=
# NOTE: can contain both..
#       .. .makefile-mini/<ignoredbinary>
#       .. <notignoredbinary>
# NOTE: filepath not "path to file" as binary is not always there before..
#       .. this variable is used
MM_FILEPATH_PER_BINARY:=

# NOTE: MM_IGNOREDBINARIES_PATTERNS == <mm_start_parameters_t>.ignoredbinaries
MM_IGNOREDBINARIES_PATTERNS:=

# NOTE: .makefile-mini/<ignoredbinary>
MM_IGNOREDBINARIES:=
# NOTE: <notignoredbinary>
MM_NOTIGNOREDBINARIES:=

# NOTE: for sanity pattern will be surrounded by ^ and $
MM_SHAREDSHADER_PATTERN:=.*.spv
MM_STATICSHADER_PATTERN:=.*.spv.h
MM_SHADER_PATTERNS:=$(MM_SHAREDSHADER_PATTERN) $(MM_STATICSHADER_PATTERN)
MM_STATICLIBRARY_PATTERNS:=.*lib.*$(MM_STATICLIBRARY_EXTENSION)
MM_SHAREDLIBRARY_PATTERNS:=.*lib.*$(MM_SHAREDLIBRARY_EXTENSION)
ifndef OS #< linux
# NOTE: [^/\\\.\n] not required as filepath cannot contain newline
MM_EXECUTABLE_PATTERNS:=.*[/\\][^/\\\.]+ [^/\\\.]+
else
MM_EXECUTABLE_PATTERNS:=.*.exe
endif

# NOTE: $(1) == variablename
define mm_start_parameters_t=
$(eval $(1).ignoredbinaries:=)
endef
# NOTE: ^
#       .ignoredbinaries == empty or pattern(s), each pattern is a regular..
#       .. expression without any space(s) allowed)

#*********************************** checks ***********************************

# NOTE: $(1) == functionname
#       $(2) == <mm_start_parameters_t>
define mm_check_start_parameters_t=
$(call mm_check_if_defined,$(1),$(2).ignoredbinaries)
endef

#******************************************************************************

# NOTE: $(1) == <mm_start_parameters_t>
define mm_start=
$(if $(filter undefined,$(origin MM_SAFETY)),,\
	$(call mm_check_start_parameters_t,$(0),$(1))\
)
$(eval MM_IGNOREDBINARIES_PATTERNS:=$$($(1).ignoredbinaries))
endef
# NOTE: ^
#       $$ in $(eval MM_IGNOREDBINARIES_PATTERN:=$$<..>) because patterns..
#       .. may contain $ which should not be directly supplied to eval

# NOTE: $(1) == binary
# NOTE: assumes $(1) does not start with .makefile-mini/
mm_is_binary_ignored=$(if $(MM_IGNOREDBINARIES_PATTERNS),$(if $(call mm_filter_using_patterns,$(MM_IGNOREDBINARIES_PATTERNS),$(1)),1,0),0)

# NOTE: $(1) == binary
mm_get_binaryfilepath_from_binary=$(if $(filter 1,$(call mm_is_binary_ignored,$(1))),.makefile-mini/$(1),$(1))

# NOTE: $(1) == binary
#       $(2) == filepath variablename
# NOTE: assumes $(1) does not start with .makefile-mini/
define mm_add_binary=
$(eval $(0)_filepath:=)
$(if $(filter 1,$(call mm_is_binary_ignored,$(1))),\
	$(eval $(0)_filepath:=.makefile-mini/$(1))\
	$(eval MM_IGNOREDBINARIES+=$(1)),\
	$(eval $(0)_filepath:=$(1))\
	$(eval MM_NOTIGNOREDBINARIES+=$(1))\
)
$(eval MM_FILEPATH_PER_BINARY+=$($(0)_filepath))
$(eval $(2):=$($(0)_filepath))
endef

# NOTE: $(1) == binarypart
define mm_get_binarypartfolderpathpart_from_binarypart=
$(strip \
	$(eval $(0)_a:=$(dir $(1)))\
	$(if $(filter ./,$($(0)_a)),,\
		$($(0)_a)\
	)\
)
endef

# NOTE: $(1) == binarypart
define mm_get_binarypartfolderpath_from_binarypart=
$(strip \
	$(eval $(0)_binarypartfolderpathpart:=$(call mm_get_binarypartfolderpathpart_from_binarypart,$(1)))\
	$(if $($(0)_binarypartfolderpathpart),\
		.makefile-mini/$($(0)_binarypartfolderpathpart)\
	,)\
)
endef

# NOTE: $(1) == binarypart
# NOTE: assumes $(1) does not start with .makefile-mini/
define mm_add_binarypart=
$(eval MM_BINARYPARTS+=$(1))
$(eval $(0)_binarypartfolderpathpart:=$(call mm_get_binarypartfolderpathpart_from_binarypart,$(1)))
$(if $(filter $($(0)_binarypartfolderpathpart),$(MM_FOLDERPATHPART_PER_BINARYPART)),,\
	$(eval MM_FOLDERPATHPART_PER_BINARYPART+=$($(0)_binarypartfolderpathpart))\
)
endef

#******************************************************************************
#                                   resources
#******************************************************************************

#define mm_info_about_resource_t=
#$(eval .name:=)
#endef

#*********************************** checks ***********************************

# NOTE: $(1) == functionname
#       $(2) == resourcetype plural
#       $(3) == RESOURCETYPE plural
#       $(4) == resources variablename
define mm_check_resources=
$(eval $(0)_a:=$(filter-out $(MM_$(3)),$($(4))))
$(if $($(0)_a),$(error $(3) contains element(s) that aren't $(2) in $(1)),)
endef

#******************************************************************************

# NOTE: $(1) == RESOURCETYPE
#       $(2) == resourcename
# NOTE: if there is a resource for which <mm_info_about_resource_t>.name ==..
#       .. $(2).. returns 1
#       otherwise.. returns 0
define mm_is_resource=
$(eval $(0)_bIsResource:=0)
$(foreach $(0)_infoAboutResource,$(MM_INFO_PER_$(1)),\
	$(if $(filter $($($(0)_infoAboutResource).name),$(2)),\
		$(eval $(0)_bIsResource:=1)\
	,)\
)
$(if $(filter 1,$($(0)_bIsResource)),1,0)
endef

# NOTE: $(1) == RESOURCETYPE
#       $(2) == variablename(s)
#       $(3) == resources
define mm_get_variables_from_resources=
$(foreach $(0)_resource,$(3),\
	$(foreach $(0)_infoAboutResource,$(MM_INFO_PER_$(1)),\
		$(if $(filter $($($(0)_infoAboutResource).name),$($(0)_resource)),\
			$(foreach $(0)_a,$(2),\
				$($($(0)_infoAboutResource).$($(0)_a))\
			)\
		,)\
	)\
)
endef

#******************************************************************************
#                                     glsl
#******************************************************************************

# NOTE: $(1) == variablename
define mm_info_about_spvasm_from_glsl_t=
$(eval $(1).type:=)
$(eval $(1).spvasm:=)
$(eval $(1).glslangValidator:=)
endef
# NOTE: ^
#       .type == one of EMMShadertype
# NOTE: "It's a convention to name SPIR-V assembly and binary files with..
#       .. suffix .spvasm and .spv, respectively",
#       https://github.com/KhronosGroup/SPIRV-Tools#command-line-tools

MM_INFO_PER_SPVASM_FROM_GLSL:=

# NOTE: both .spvasm2 from .spvasm and .spv2 from .spvasm2 are temporary..
#       .. until possible to rename entrypoints using spirv-link,..
#       .. https://github.com/KhronosGroup/glslang/issues/605
define mm_info_about_spvasm2_and_spv2_from_spvasm_t=
$(eval $(1).a:=)
endef
# NOTE: ^
#       <a>.spvasm2
#       <a>.spv2

MM_INFO_PER_SPVASM2_AND_SPV2_FROM_SPVASM:=

#*********************************** shader ***********************************

EMMShadertype:=EMMShadertype_Vertex EMMShadertype_Pixel

# NOTE: for consistency with libraries a shader/shaderlibary is..
#       .. shared if it is loaded at runtime
#       .. static if it is for compiling into a library/executable
EMMShaderfiletype:=EMMShaderfiletype_Shared EMMShaderfiletype_Static
EMMShaderfiletype_All:=$(EMMShaderfiletype)

# NOTE: $(1) == variablename
define mm_add_shader_parameters_t=
$(eval $(1).filetypes:=$(EMMShaderfiletype_All))
$(eval $(1).type:=)
$(eval $(1).glsl:=)
$(eval $(1).glslangValidator:=)
endef
# NOTE: ^
#       .filetypes == one or more of EMMShaderfiletype, may not be empty
#       .glsl == <filepath><filename>.glsl, must contain one element
#       .glslangValidator == i.e. glslangValidator <.glslangValidator>, may..
#       .. be empty

# NOTE: $(1) == variablename
define mm_info_about_shader_t=
$(eval $(1).name:=)
$(eval $(1).type:=)
$(eval $(1).filetypes:=)
$(eval $(1).spvasm:=)
$(eval $(1).spv:=)
$(eval $(1).spvFilepath:=)
$(eval $(1).spvHFilepath:=)
endef
# NOTE: .name == output files are..
#                .. <.name>.spv
#                .. <.name>.spv.h -> char <$(1).spv>_spv = { <...> };
#       .spv == is for mm_get_binarypartfolderpath_from_binarypart

MM_INFO_PER_SHADER:=

#********************************** checks  ***********************************

# NOTE: $(1) == functionname
#       $(2) == <mm_add_shader_parameters_t>
define mm_check_add_shader_parameters_t=
$(call mm_check_if_defined,$(1),$(2).filetypes)
$(call mm_check_if_defined,$(1),$(2).type)
$(call mm_check_if_defined,$(1),$(2).glsl)
$(call mm_check_if_defined,$(1),$(2).glslangValidator)

$(if $($(2).filetypes),,$(error $(2).filetypes may not be empty in $(1)))
$(call mm_check_if_valid_values,$(1),$(EMMShaderfiletype_All),$(2).filetypes)

$(if $($(2).type),,$(error $(2).type may not be empty in $(1)))
$(call mm_check_if_valid_value,$(1),$(EMMShadertype),,$(2).type)

$(if $(filter-out 1,$(words $($(2).glsl))),$(error $(2).glsl must contain one element in $(1)),)
$(call mm_check_if_valid_value,$(1),%.glsl,$(2).glsl)
endef

#******************************************************************************

# NOTE: $(1) == shadername
mm_is_shader=$(call mm_is_resource,SHADER,$(1))

# NOTE: $(1) == shadername
#       $(2) == <mm_add_shader_parameters_t>
# NOTE: binary is $(1).spvasm for every shadertype thus shadername is shared..
#       .. between shadertypes
define mm_add_shader=
$(if $(filter undefined,$(origin MM_SAFETY)),,\
	$(if $(filter 1,$(call mm_is_shader,$(1))),$(error attempted to add shader $(1) more than once in $(0)),)\
	$(call mm_check_add_shader_parameters_t,$(0),$(2))\
)
$(call mm_add_or_append_one_element,MM_INFO_PER_SHADER)
$(eval $(0)_infoAboutShader:=$(lastword $(MM_INFO_PER_SHADER)))
$(call mm_info_about_shader_t,$($(0)_infoAboutShader))
$(eval $($(0)_infoAboutShader).name:=$(1))
$(eval $($(0)_infoAboutShader).type:=$($(2).type))
$(eval $($(0)_infoAboutShader).filetypes:=$($(2).filetypes))
$(eval $($(0)_infoAboutShader).spvasm:=$(patsubst %.glsl,%.spvasm,$($(2).glsl)))
$(eval $($(0)_infoAboutShader).spv:=$($(1).spv))
$(eval $(0)_bIsSpvasmFromGlsl:=0)
$(foreach $(0)_infoAboutSpvasmFromGlsl,$(MM_INFO_PER_SPVASM_FROM_GLSL),\
	$(if $(filter $($($(0)_infoAboutSpvasmFromGlsl).spvasm),$($(2).spvasm)),\
		$(if $(filter $($($(0)_infoAboutSpvasmFromGlsl).type),$($(2).type)),,\
			$(error $($(2).spvasm) required more than once but with different type value in $(0))\
		)\
		$(if $(filter 0,$(call mm_equals,$($($(0)_infoAboutSpvasmFromGlsl).glslangValidator).$($(2).glslangValidator)))\
			$(error $($(2).spvasm) required more than once but with different glslangValidator value in $(0))\
		,)\
		$(eval $(0)_bIsSpvasmFromGlsl:=1)\
	,)\
)
$(if $(filter 0,$($(0)_bIsSpvasmFromGlsl)),\
	$(call mm_add_or_append_one_element,MM_INFO_PER_SPVASM_FROM_GLSL)\
	$(eval $(0)_infoAboutSpvasmFromGlsl:=$(lastword $(MM_INFO_PER_SPVASM_FROM_GLSL)))\
	$(eval $($(0)_infoAboutSpvasmFromGlsl).type:=$($(2).type))\
	$(eval $($(0)_infoAboutSpvasmFromGlsl).spvasm:=$($($(0)_infoAboutShader).spvasm))\
	$(eval $($(0)_infoAboutSpvasmFromGlsl).glslangValidator:=$($(2).glslangValidator))\
	$(call mm_add_binarypart,$($($(0)_infoAboutShader).spvasm))\
,)
$(if $(filter EMMShaderfiletype_Shared,$($(2).filetypes)),\
	$(call mm_add_binary,$(1).spv,$($(0)_infoAboutShader).spvFilepath),\
	$(call mm_add_binarypart,$(1).spv)\
	$(eval $($(0)_infoAboutShader).spvFilepath:=.makefile-mini/$(1).spv)\
)
$(if $(filter EMMShaderfiletype_Static,$($(2).filetypes)),\
	$(call mm_add_binary,$(1).spv.h,$($(0)_infoAboutShader).spvHFilepath)\
,)
endef
# TODO: ^
#       if not EMMShaderfiletype_Shared.. .makefile-mini/$(1).spv
#       otherwise.. $(1).spv

#******************************************************************************
#                                    c/c++
#******************************************************************************

# NOTE: $(1) == variablename
define mm_info_about_o_from_c_t=
$(eval $(1).c:=)
$(eval $(1).hFolders:=)
$(eval $(1).gcc:=)
$(eval $(1).o:=)
endef
# NOTE: ^
#       .o == if windows.. <.c>.o
#             if linux..
#             .. if sharedlibrary.. <.c>.shared.o
#             .. otherwise.. <.c>.static.o

MM_INFO_PER_O_FROM_C:=

# NOTE: $(1) == variablename
define mm_info_about_o_from_cpp_t=
$(eval $(1).cpp:=)
$(eval $(1).hppFolders:=)
$(eval $(1).g++:=)
$(eval $(1).o:=)
endef
# NOTE: ^
#       .o == if windows.. <.cpp>.o
#             if linux..
#             .. if sharedlibrary.. <.cpp>.shared.o
#             .. otherwise.. <.cpp>.static.o

MM_INFO_PER_O_FROM_CPP:=

#********************************** checks ***********************************

# NOTE: $(1) == functionname
#       $(2) == lib
define mm_check_lib=
$(eval $(0)_invalidLib:=)
$(foreach $(0)_lib,$($(2)),\
	$(if $(findstring .,$($(0)_lib)),\
		$(eval $(0)_invalidLib+=$($(0)_lib))\
	)\
)
$(if $($(0)_invalidLib),\
	$(error $(2) contains invalid value(s) $($(0)_invalidLib) in $(1))\
,)
endef

#******************************************************************************

# NOTE: $(1) == functionname
#       $(2) == C/CPP
#       $(3) == c/cpp
#       $(4) == h/hpp
#       $(5) == gcc/g++
#       $(6) == .hFolders/.hppFolders
#       $(7) == .gcc/.g++
#       $(8) == .o
define mm_add_o_from_c_or_cpp=
$(foreach $(0)_o,$(8),\
	$(if $(OS),\
		$(eval $(0)_cOrCpp:=$(basename $($(0)_o))),\
		$(eval $(0)_cOrCpp:=$(basename $(basename $($(0)_o))))\
	)\
	$(eval $(0)_gccOrG++:=$(7))\
	$(if $(OS),,\
		$(if $(filter %.shared.o,$($(0)_o)),\
			$(eval $(0)_gccOrG++ +=-fpic -fvisibility=hidden)\
		,)\
	)\
	$(eval $(0)_bIsOFromCOrCpp:=0)\
	$(foreach $(0)_infoAboutOFromCOrCpp,$(MM_INFO_PER_O_FROM_$(2)),\
		$(if $(filter $($($(0)_infoAboutOFromCOrCpp).o),$($(0)_o)),\
			$(if $(filter 0,$(call mm_equals,$($($(0)_infoAboutOFromCOrCpp).$(4)Folders),$(6))),\
				$(error $($(0)_o) required more than once but with different $(4)Folders value in $(1))\
			,)\
			$(if $(filter 0,$(call mm_equals,$($($(0)_infoAboutOFromCOrCpp).$(5)),$($(0)_gccOrG++))),\
				$(error $($(0)_o) required more than once but with different $(5) value in $(1))\
			,)\
			$(eval $(0)_bIsOFromCOrCpp:=1)\
		,)\
	)\
	$(if $(filter 0,$($(0)_bIsOFromCOrCpp)),\
		$(call mm_add_or_append_one_element,MM_INFO_PER_O_FROM_$(2))\
		$(eval $(0)_infoAboutOFromCOrCpp:=$(lastword $(MM_INFO_PER_O_FROM_$(2))))\
		$(call mm_info_about_o_from_c_or_cpp_t,$($(0)_infoAboutOFromCOrCpp))\
		$(eval $($(0)_infoAboutOFromCOrCpp).$(3):=$($(0)_cOrCpp))\
		$(eval $($(0)_infoAboutOFromCOrCpp).$(4)Folders:=$(6))\
		$(eval $($(0)_infoAboutOFromCOrCpp).$(5):=$($(0)_gccOrG++))\
		$(eval $($(0)_infoAboutOFromCOrCpp).o:=$($(0)_o))\
		$(call mm_add_binarypart,$($(0)_o))\
	,)\
)
endef
# NOTE: ^
#       though not possible that "-fpic -fvisibilty=hidden" for .shared.o is..
#       .. ever an issue, adding it here before checking for sanity

# NOTE: $(1) == functionname
#       $(2) == .hFolders
#       $(3) == .gcc
#       $(4) == .o
mm_add_o_from_c=$(call mm_add_o_from_c_or_cpp,$(1),C,c,h,gcc,$(2),$(3),$(4))

# NOTE: $(1) == functionname
#       $(2) == .hppFolders
#       $(3) == .g++
#       $(4) == .o
mm_add_o_from_cpp=$(call mm_add_o_from_c_or_cpp,$(1),CPP,cpp,hpp,g++,$(2),$(3),$(4))

#********************************** library ***********************************

# NOTE: library and shaderlibrary are separate as every library can be built..
#       .. from the same files and every shaderlibrary can be built from the..
#       .. same files
EMMLibraryfiletype:=EMMLibraryfiletype_Static EMMLibraryfiletype_Shared
EMMLibraryfiletype_All:=$(EMMLibraryfiletype)

# NOTE: $(1) == <mm_add_library_parameters_t>
# NOTE: if .filetypes is empty.. .c and .gcc must be empty
define mm_add_library_parameters_t=
$(eval $(1).filetypes:=)
$(eval $(1).c:=)
$(eval $(1).localC:=)
$(eval $(1).cpp:=)
$(eval $(1).localCpp:=)
$(eval $(1).h:=)
$(eval $(1).hpp:=)
$(eval $(1).hFolders:=)
$(eval $(1).hppFolders:=)
$(eval $(1).hAndHppFolders:=)
$(eval $(1).lib:=)
$(eval $(1).libFolders:=)
$(eval $(1).cGcc:=)
$(eval $(1).cppG++:=)
$(eval $(1).gccOrG++:=)
$(eval $(1).libraries:=)
$(eval $(1).staticlibraries:=)
$(eval $(1).sharedlibraries:=)
endef
# NOTE: ^
#       .filetypes == empty (i.e. .h only) or one or multiple of..
#                     .. EMMLibraryfiletype
#       .localC == .c file(s) for which objdump would report l on the..
#                  .. corresponding .o for every extern symbol
#       .localCpp == .cpp file(s) for which objdump would report l on the..
#                    .. corresponding .o for every extern symbol
#       .hFolders == folders only for c, equivalent to -I for gcc
#       .hppFolders == folders only for c++, equivalent to -I for g++
#       .hAndHppFolders == folders for both c and c++
#       .libraries == libraryname(s) and/or external project address(es)..
#                     .. each to a library (i.e. <projectname>:<libraryname>)
#       .lib == lib as in the first lib in lib<...>.<lib/dll/a/so>,..
#       .. equivalent to -l for gcc
#       .libFolders == folders, equivalent to -L for gcc
#       .gccOrG++ == if .cpp is empty.. gcc <.gcc> <...>
#                    otherwise.. g++ <.g++> <....
# NOTE: header only library (empty .filetypes) is such that external project..
#       .. address (<projectname>:<libraryname>) to header only library is..
#       .. possible
# NOTE: ^
#       current limitation of .local<C/Cpp> is that static variables and..
#       .. functions can only occur once across all local files because..
#       .. every local .o is merged into one .o
# TODO: ^
#       option would be to mangle static symbols per file, but don't know..
#       .. how to do that using windows+mingw/linux a.t.m.

# NOTE: $(1) == variablename
define mm_info_about_library_t=
$(eval $(1).name:=)
$(eval $(1).filetypes:=)
$(eval $(1).o:=)
$(eval $(1).staticO:=)
$(eval $(1).sharedO:=)
$(eval $(1).localStaticO:=)
$(eval $(1).h:=)
$(eval $(1).hpp:=)
$(eval $(1).lib:=)
$(eval $(1).libFolders:=)
$(eval $(1).cc:=)
$(eval $(1).gccOrG++:=)
$(eval $(1).otherLibraries:=)
$(eval $(1).otherStaticlibraries:=)
$(eval $(1).otherSharedlibraries:=)
$(if $(OS),\
	$(eval $(1).windows.libfilepath:=)\
	$(eval $(1).windows.dllfilepath:=),\
	$(eval $(1).linux.afilepath:=)\
	$(eval $(1).linux.sofilepath:=)\
)
$(eval $(1).hAndHppFilepathPerOtherLibrary:=)
$(eval $(1).binaryfilepathPerOtherStaticlibrary:=)
$(eval $(1).binaryfilepathPerOtherSharedlibrary:=)
endef
# NOTE: ^
#       .o == if windows.. .o from .c
#             if linux.. <.staticO> <.sharedO>
#       .staticO == if windows.. <.o>
#                    if linux.. compiled w.o. -fpic -fvisibility=hidden
#       .sharedO == if windows.. <.o>
#                    if linux.. == compiled w. -fpic -fvisibility=hidden
#       .localStaticO == .o, .staticO and .sharedO already include every..
#                        .. local static .o, .localStaticO contains only..
#                        .. those .o file(s) if any in .staticO that are local
#       .cc == gcc/g++
#       .windows.<lib/dll>filepath == filepath to <.lib/.dll> binary
#       .linux.<a/so>filepath == filepath to <.a/.so> binary

MM_INFO_PER_LIBRARY:=

MM_LIBRARIES:=

#*********************************** checks ***********************************

# NOTE: $(1) == functionname
#       $(2) == libraries variablename
mm_check_libraries=$(call mm_check_resources,$(1),libraries,LIBRARIES,$(2))

# NOTE: $(1) == functionname
#       $(2) == <mm_add_*_parameters_t>
define mm_check_libraries_and_staticlibraries_and_sharedlibraries=
$(call mm_check_libraries,$(1),$(2).libraries)
$(call mm_check_libraries,$(1).$(2).staticlibraries)
$(call mm_check_libraries,$(1).$(2).sharedlibraries)
$(eval $(0)_a:=$(filter $($(2).staticlibraries),$($(2).libraries)))
$(if $($(0)_a),$(error $($(0)_a) specified both in $(2).libraries and $(2).staticlibraries) in $(1),)
$(eval $(0)_b:=$(filter $($(2).sharedlibraries),$($(2).libraries)))
$(if $($(0)_b),$(error $($(0)_b) specified both in $(2).libraries and $(2).sharedlibaries) in $(1),)
endef

# NOTE: $(1) == functionname
#       $(2) == <mm_add_library_parameters_t>
define mm_check_add_library_parameters_t=
$(call mm_check_if_defined,$(1),$(2).filetypes)
$(call mm_check_if_defined,$(1),$(2).c)
$(call mm_check_if_defined,$(1),$(2).localC)
$(call mm_check_if_defined,$(1),$(2).cpp)
$(call mm_check_if_defined,$(1),$(2).localCpp)
$(call mm_check_if_defined,$(1),$(2).h)
$(call mm_check_if_defined,$(1),$(2).hpp)
$(call mm_check_if_defined,$(1),$(2).hFolders)
$(call mm_check_if_defined,$(1),$(2).hppFolders)
$(call mm_check_if_defined,$(1),$(2).hAndHppFolders)
$(call mm_check_if_defined,$(1),$(2).lib)
$(call mm_check_if_defined,$(1),$(2).libFolders)
$(call mm_check_if_defined,$(1),$(2).cGcc)
$(call mm_check_if_defined,$(1),$(2).cppG++)
$(call mm_check_if_defined,$(1),$(2).gccOrG++)
$(call mm_check_if_defined,$(1),$(2).libraries)
$(call mm_check_if_defined,$(1),$(2).staticlibraries)
$(call mm_check_if_defined,$(1),$(2).sharedlibraries)

$(if $($(2).filetypes),\
	$(call mm_check_if_valid_values,$(1),$(EMMLibraryfiletype_All),$(2).filetypes)\
	$(if $($(2).c) $($(2).cpp),,$(error if $(2).filetypes is not empty.. $(2).c and $(2).cpp may not both be empty in $(1)))\
	$(call mm_check_if_valid_values,$(1),%.c,$(2).c)\
	$(call mm_check_if_valid_values,$(1),%.cpp,$(2).cpp),\
	$(if $($(2).c),$(error if $(2).filetypes is empty.. $(2).c must be empty in $(1)),)\
	$(if $($(2).cpp),$(error if $(2).filetypes is empty.. $(2).cpp must be empty in $(1)),)\
	$(if $($(2).h) $($(2).hpp),,$(error if $(2).filetypes is empty.. $(2).h and $(2).hpp may not both be empty in $(1)))\
	$(if $($(2).cGcc),$(error if $(2).filetypes is empty.. $(2).cGcc must be empty in $(1)),)\
	$(if $($(2).cppG++),$(error if $(2).filetypes is empty.. $(2).cppG++ must be empty in $(1)),)\
	$(if $($(2).gccOrG++),$(error if $(2).filetypes is empty.. $(2).gccOrG++ must be empty in $(1)),)\
)
$(call mm_check_if_valid_values,$(1),%.h,$(2).h)
$(call mm_check_if_valid_values,$(1),%.hpp,$(2).hpp)
$(call mm_check_lib,$(1),$(2).lib)
$(call mm_check_libraries_and_staticlibraries_and_sharedlibraries,$(1),$(2))
endef
# TODO: ^
#       implement .h, .lib, .libFolders, .windows.*, .linux.*

#******************************************************************************

# NOTE: $(1) == libraryname
mm_is_library=$(call mm_is_resource,LIBRARY,$(1))

# NOTE: $(1) == libraries
mm_get_filepath_per_h_and_hpp_from_libraries=$(call mm_get_variables_from_resources,LIBRARY,h hpp,$(1))

# NOTE: $(1) == staticlibraries
mm_get_filepath_per_binary_from_staticlibraries=$(call mm_get_variables_from_resources,LIBRARY,$(MM_OS)$(MM_STATICLIBRARY_EXTENSION)filepath,$(1))
# NOTE: $(1) == sharedlibraries
mm_get_filepath_per_binary_from_sharedlibraries=$(call mm_get_variables_from_resources,LIBRARY,$(MM_OS)$(MM_SHAREDLIBRARY_EXTENSION)filepath,$(1))

# NOTE: $(1) == libraryname
#       $(2) == <mm_add_library_parameters_t>
define mm_add_library=
$(if $(filter undefined,$(origin MM_SAFETY)),,\
	$(if $(filter 1,$(call mm_is_library,$(1))),$(error attempted to add library $(1) more than once in $(0)),)\
	$(call mm_check_add_library_parameters_t,$(0),$(2))\
)
$(eval MM_INFO_PER_LIBRARY+=MM_INFO_PER_LIBRARY.$(words $(MM_INFO_PER_LIBRARY)))
$(eval $(0)_infoAboutLibrary:=$(lastword $(MM_INFO_PER_LIBRARY)))
$(call mm_info_about_library_t,$($(0)_infoAboutLibrary))
$(eval $($(0)_infoAboutLibrary).name:=$(1))
$(eval $($(0)_infoAboutLibrary).filetypes:=$($(2).filetypes))
$(eval $(0)_oFromC:=)
$(eval $(0)_oFromLocalC:=)
$(eval $(0)_oFromCpp:=)
$(eval $(0)_oFromLocalCpp:=)
$(if $(OS),\
	$(eval $(0)_oFromC:=$(addsuffix .o,$($(2).c)))\
	$(eval $(0)_oFromLocalC:=$(addsuffix .o,$($(2).localC)))\
	$(eval $(0)_oFromCpp:=$(addsuffix .o,$($(2).cpp)))\
	$(eval $(0)_oFromLocalCpp:=$(addsuffix .o,$($(2).localCpp)))\
	$(eval $($(0)_infoAboutLibrary).o:=$($(0)_oFromC) $($(0)_oFromLocalC) $($(0)_oFromCpp) $($(0)_oFromLocalCpp))\
	$(eval $($(0)_infoAboutLibrary).staticO:=$($($(0)_infoAboutLibrary).o))\
	$(eval $($(0)_infoAboutLibrary).sharedO:=$($($(0)_infoAboutLibrary).o)),\
	$(if $(filter EMMLibraryfiletype_Static,$($($(0)_infoAboutLibrary).filetypes)),\
		$(eval $(0)_staticOFromC:=$(addsuffix .static.o,$($(2).c)))\
		$(eval $(0)_staticOFromLocalC:=$(addsuffix .static.o,$($(2).localC)))\
		$(eval $(0)_staticOFromCpp:=$(addsuffix .static.o,$($(2).cpp)))\
		$(eval $(0)_staticOFromLocalCpp:=$(addsuffix .static.o,$($(2).localCpp)))\
		$(eval $($(0)_infoAboutLibrary).staticO:=$($(0)_staticOFromC) $($(0)_staticOFromLocalC) $($(0)_staticOFromCpp) $($(0)_staticOFromLocalCpp))\
		$(eval $($(0)_infoAboutLibrary).o+=$($($(0)_infoAboutLibrary).staticO))\
		$(eval $(0)_oFromC+=$($(0)_staticOFromC))\
		$(eval $(0)_oFromLocalC+=$($(0)_staticOFromLocalC))\
		$(eval $(0)_oFromCpp+=$($(0)_staticOFromCpp))\
		$(eval $(0)_oFromLocalCpp+=$($(0)_staticOFromLocalCpp))\
	,)\
	$(if $(filter EMMLibraryfiletype_Shared,$($($(0)_infoAboutLibrary).filetypes)),\
		$(eval $(0)_sharedOFromC:=$(addsuffix .shared.o,$($(2).c)))\
		$(eval $(0)_sharedOFromLocalC:=$(addsuffix .shared.o,$($(2).localC)))\
		$(eval $(0)_sharedOFromCpp:=$(addsuffix .shared.o,$($(2).cpp)))\
		$(eval $(0)_sharedOFromLocalCpp:=$(addsuffix .shared.o,$($(2).localCpp)))\
		$(eval $($(0)_infoAboutLibrary).sharedO:=$($(0)_sharedOFromC) $($(0)_sharedOFromLocalC) $($(0)_sharedOFromCpp) $($(0)_sharedOFromLocalCpp))\
		$(eval $($(0)_infoAboutLibrary).o+=$($($(0)_infoAboutLibrary).sharedO))\
		$(eval $(0)_oFromC+=$($(0)_sharedOFromC))\
		$(eval $(0)_oFromLocalC+=$($(0)_sharedOFromLocalC))\
		$(eval $(0)_oFromCpp+=$($(0)_sharedOFromCpp))\
		$(eval $(0)_oFromLocalCpp+=$($(0)_sharedOFromLocalCpp))\
	,)\
)
$(eval $($(0)_infoAboutLibrary).localStaticO:=$($(0)_staticOFromLocalC) $($(0)_staticOFromLocalCpp))
$(eval $($(0)_infoAboutLibrary).h:=$($(2).h))
$(eval $($(0)_infoAboutLibrary).hpp:=$($(2).hpp))
$(eval $($(0)_infoAboutLibrary).cc:=$(if $($(2).cpp),g++,gcc))
$(eval $($(0)_infoAboutLibrary).otherLibraries:=$(filter $($(2).libraries),$(MM_LIBRARIES)))
$(eval $($(0)_infoAboutLibrary).otherStaticlibraries:=$(filter $($(2).staticlibraries),$(MM_STATICLIBRARIES)))
$(eval $($(0)_infoAboutLibrary).otherSharedlibraries:=$(filter $($(2).sharedlibraries),$(MM_SHAREDLIBRARIES)))
$(eval $($(0)_infoAboutLibrary).hAndHppFilepathPerOtherLibrary:=$(call mm_get_filepath_per_h_and_hpp_from_libraries,$($($(0)_infoAboutLibrary).otherLibraries)))
$(eval $($(0)_infoAboutLibrary).binaryfilepathPerOtherStaticlibrary:=$(call mm_get_filepath_per_binary_from_staticlibraries,$($($(0)_infoAboutLibrary).otherStaticlibraries) $($($(0)_infoAboutLibrary).otherLibraries)))
$(eval $($(0)_infoAboutLibrary).binaryfilepathPerOtherSharedlibrary:=$(call mm_get_filepath_per_binary_from_sharedlibraries,$($($(0)_infoAboutLibrary).otherSharedlibraries) $($($(0)_infoAboutLibrary).otherLibraries)))
$(eval $(0)_a:=$(sort $(dir $($($(0)_infoAboutLibrary).hAndHppFilepathPerOtherLibrary))))
$(if $($(0)_oFromC),\
	$(call mm_add_o_from_c,$(0),$($(0)_a) $($(2).hAndHppFolders) $($(2).hFolders),$($(2).cGcc),$($(0)_oFromC) $($(0)_oFromLocalC))\
,)
$(if $($(0)_oFromCpp),\
	$(call mm_add_o_from_cpp,$(0),$($(0)_a) $($(2).hAndHppFolders) $($(2).hppFolders),$($(2).cppG++),$($(0)_oFromCpp) $($(0)_oFromLocalCpp))\
,)
$(if $(filter EMMLibraryfiletype_Static,$($(2).filetypes)),\
	$(if $(strip $($($(0)_infoAboutLibrary).localStaticO)),\
		$(call mm_add_binarypart,lib$(1)$(MM_STATICLIBRARY_EXTENSION).nm)\
		$(call mm_add_binarypart,lib$(1)$(MM_STATICLIBRARY_EXTENSION).o)\
	,)\
	$(call mm_add_binary,lib$(1)$(MM_STATICLIBRARY_EXTENSION),$($(0)_infoAboutLibrary).$(MM_OS)$(MM_STATICLIBRARY_EXTENSION)filepath)\
,)
$(if $(filter EMMLibraryfiletype_Shared,$($(2).filetypes)),\
	$(call mm_add_binary,lib$(1)$(MM_SHAREDLIBRARY_EXTENSION),$($(0)_infoAboutLibrary).$(MM_OS)$(MM_SHAREDLIBRARY_EXTENSION)filepath)\
,)
$(eval MM_LIBRARIES+=$(1))
endef
# NOTE: ^
#       sort after mm_add_library_b:= is for removing duplicates only
# TODO: mm_not_add_library and mm_add_library=$(eval $(call mm_not_add_library,<...>))?
#       ^
#       to allow for comments in mm_not_add_library?
# NOTE: ^
#       for shared library .localC is identical to .c
#       this is fine as default visibility for .shared.o is hidden?

#********************************* executable *********************************

# NOTE: default executablefiletype is..
#       .. if windows.. .exe
#       .. if linux.. <empty>
# NOTE: additionalexecutablefiletypes..
#       .. portable -> .exe/.AppImage
EMMAdditionalexecutablefiletype:=EMMAdditionalexecutablefiletype_Portable
EMMAdditionalexecutablefiletype_All:=$(EMMAdditionalexecutablefiletype)

define mm_add_executable_parameters_t
$(eval $(1).additionalfiletypes:=)
$(eval $(1).c:=)
$(eval $(1).cpp:=)
$(eval $(1).hFolders:=)
$(eval $(1).hppFolders:=)
$(eval $(1).hAndHppFolders:=)
$(eval $(1).lib:=)
$(eval $(1).libFolders:=)
$(eval $(1).cGcc:=)
$(eval $(1).cppG++:=)
$(eval $(1).gccOrG++:=)
$(eval $(1).libraries:=)
$(eval $(1).staticlibraries:=)
$(eval $(1).sharedlibraries:=)
endef

define mm_info_about_executable_t=
$(eval $(1).name:=)
$(eval $(1).additionalfiletypes:=)
$(eval $(1).o:=)
$(eval $(1).lib:=)
$(eval $(1).libFolders:=)
$(eval $(1).cc:=)
$(eval $(1).gccOrG++:=)
$(eval $(1).libraries:=)
$(eval $(1).staticlibraries:=)
$(eval $(1).sharedlibraries:=)
$(if $(OS),\
	$(eval $(1).windows.exefilepath:=),\
	$(eval $(1).linux.filepath:=)\
	$(eval $(1).linux.AppImagefilepath:=)
)
$(eval $(1).hAndHppFilepathPerLibrary:=)
$(eval $(1).binaryfilepathPerStaticlibrary:=)
$(eval $(1).binaryfilepathPerSharedlibrary:=)
endef
# NOTE: ^
#       .cc == gcc/g++
#       .linux.filepath == filepath to executable without extension (<no..
#       .. extension>)
#       .linux.AppImagefilepath == exception to "appimagefilepath" for..
#       .. consistency

MM_INFO_PER_EXECUTABLE:=

#*********************************** checks ***********************************

# NOTE: $(1) == functionname
#       $(2) == <mm_add_executable_parameters_t>
define mm_check_add_executable_parameters_t=
$(call mm_check_if_defined,$(1),$(2).additionalfiletypes)
$(call mm_check_if_defined,$(1),$(2).c)
$(call mm_check_if_defined,$(1),$(2).cpp)
$(call mm_check_if_defined,$(1),$(2).hFolders)
$(call mm_check_if_defined,$(1),$(2).hppFolders)
$(call mm_check_if_defined,$(1),$(2).hAndHppFolders)
$(call mm_check_if_defined,$(1),$(2).lib)
$(call mm_check_if_defined,$(1),$(2).libFolders)
$(call mm_check_if_defined,$(1),$(2).cGcc)
$(call mm_check_if_defined,$(1),$(2).cppG++)
$(call mm_check_if_defined,$(1),$(2).gccOrG++)
$(call mm_check_if_defined,$(1),$(2).libraries)
$(call mm_check_if_defined,$(1),$(2).staticlibraries)
$(call mm_check_if_defined,$(1),$(2).sharedlibraries)

$(call mm_check_if_valid_values,$(1),$(EMMAdditionalexecutablefiletypes_All),$(2).additionalfiletypes)
$(call mm_check_if_valid_values,$(1),%.c,$(2).c)
$(call mm_check_if_valid_values,$(1),%.cpp,$(2).cpp)
$(call mm_check_lib,$(1),$(2).lib)
$(call mm_check_libraries_and_staticlibraries_and_sharedlibraries,$(1),$(2))
endef

#******************************************************************************

# NOTE: $(1) == executablename
mm_is_executable=$(call mm_is_resource,EXECUTABLE,$(1))

# NOTE: $(1) == executablename
#       $(2) == <mm_add_executable_parameters_t>
define mm_add_executable=
$(if $(filter undefined,$(origin MM_SAFETY)),,\
	$(if $(filter 1,$(call mm_is_executable,$(1))),$(error attempted to add executable $(1) more than once in $(0)),)\
	$(call mm_check_add_executable_parameters_t,$(0),$(2))\
)
$(eval MM_INFO_PER_EXECUTABLE+=MM_INFO_PER_EXECUTABLE.$(words $(MM_INFO_PER_EXECUTABLE)))
$(eval $(0)_infoAboutExecutable:=$(lastword $(MM_INFO_PER_EXECUTABLE)))
$(eval $($(0)_infoAboutExecutable).name:=$(1))
$(if $(OS),\
	$(eval $(0)_oFromC:=$(addsuffix .o,$($(2).c)))\
	$(eval $(0)_oFromCpp:=$(addsuffix .o,$($(2).cpp))),\
	$(eval $(0)_oFromC:=$(addsuffix .static.o,$($(2).c)))\
	$(eval $(0)_oFromCpp:=$(addsuffix .static.o,$($(2).cpp)))\
)
$(eval $($(0)_infoAboutExecutable).o:=$($(0)_oFromC) $($(0)_oFromCpp))
$(eval $($(0)_infoAboutExecutable).libraries:=$(filter $($(2).libraries),$(MM_LIBRARIES)))
$(eval $($(0)_infoAboutExecutable).staticlibraries:=$(filter $($(2).staticlibraries),$(MM_STATICLIBARIES)))
$(eval $($(0)_infoAboutExecutable).sharedlibraries:=$(filter $($(2).sharedlibraries),$(MM_SHAREDLIBRARIES)))
$(eval $($(0)_infoAboutExecutable).hAndHppFilepathPerLibrary:=$(call mm_get_filepath_per_h_and_hpp_from_libraries,$($($(0)_infoAboutExecutable).libraries)))
$(eval $($(0)_infoAboutExecutable).binaryfilepathPerStaticlibrary:=$(call mm_get_filepath_per_binary_from_staticlibraries,$($($(0)_infoAboutExecutable).staticlibraries) $($($(0)_infoAboutExecutable).libraries)))
$(eval $($(0)_infoAboutExecutable).binaryfilepathPerSharedlibrary:=$(call mm_get_filepath_per_binary_from_sharedlibraries,$($($(0)_infoAboutExecutable).sharedlibraries) $($($(0)_infoAboutExecutable).libraries)))
$(eval $(0)_a:=$(patsubst lib%$(MM_STATICLIBRARY_EXTENSION),%,$(notdir $($($(0)_infoAboutExecutable).binaryfilepathPerStaticlibrary))))
$(eval $(0)_b:=$(patsubst lib%$(MM_SHAREDLIBRARY_EXTENSION),%,$(notdir $($($(0)_infoAboutExecutable).binaryfilepathPerSharedlibrary))))
$(eval $(0)_c:=$($(0)_a) $($(0)_b))
$(eval $($(0)_infoAboutExecutable).lib:=$($(0)_c) $($(2).lib))
$(eval $(0)_d:=$($($(0)_infoAboutExecutable).binaryfilepathPerStaticlibrary) $($($(0)_infoAboutExecutable).binaryfilepathPerSharedlibrary))
$(eval $(0)_e:=$(sort $(dir $($(0)_d))))
$(eval $($(0)_infoAboutExecutable).libFolders:=$($(0)_e) $($(2).libFolders))
$(eval $($(0)_infoAboutExecutable).cc:=$(if $($(2).cpp),g++,gcc))
$(eval $($(0)_infoAboutExecutable).gccOrG++:=$($(2).gccOrG++))
$(eval $(0)_f:=$(sort $(dir $($($(0)_infoAboutExecutable).hAndHppFilepathPerLibrary))))
$(if $($(0)_oFromC),\
	$(call mm_add_o_from_c,$(0),$($(0)_f) $($(2).hAndHppFolders) $($(2).hFolders),$($(2).cGcc),$($(0)_oFromC))\
,)
$(if $($(0)_oFromCpp),\
	$(call mm_add_o_from_cpp,$(0),$($(0)_f) $($(2).hAndHppFolders) $($(2).hppFolders),$($(2).cppG++),$($(0)_oFromCpp))\
,)
$(call mm_add_binary,$(1)$(MM_EXECUTABLE_EXTENSION),$($(0)_infoAboutExecutable).$(MM_OS)$(MM_EXECUTABLE_EXTENSION_OR_DOT)filepath)
$(if $(OS),,\
	$(if $(filter EMMAdditionalexecutablefiletypes_Portable,$($(2).additionalfiletypes)),\
		$(call mm_add_binary,$(1).AppImage,$(2).linux.AppImagefilepath)\
	,)\
)
endef
# NOTE: ^
#       mm_add_executable_e makes sure in libFolders makes sure that -L./..
#       .. (which is required for gcc) is supplied to gcc too
# NOTE: ^
#       sort after mm_add_executable_e:= is only for removing duplicates
# TODO: ^
#       $(filter-out $($(2).libraries),$(MM_LIBRARIES)) should be all..
#       .. assumed to be external, there should be no check in..
#       .. mm_check_add_executable_parameters_t required as will error..
#       .. automatically?

#******************************************************************************
#                                    tests
#******************************************************************************

# NOTE: $(1) == variablename
define mm_add_test_parameters_t=
$(eval $(1).executables:=)
$(eval $(1).scripts:=)
endef
# NOTE: ^
#       .scripts == if windows.. <script>.bat
#                   if linux.. <script>
# NOTE: "An executable file starting with an interpreter directive is [...]..
#       .. called a script",..
#       .. https://en.wikipedia.org/wiki/Shebang_(Unix)#Etymology
# NOTE: "A batch file is a script file",..
#       .. https://en.wikipedia.org/wiki/Batch_file
# NOTE: all executables and scripts for a test are started in parallel and..
#       .. the test is only done once all executables and scripts have..
#       .. exited (or if any executable/script fails)

# NOTE: $(1) == variablename
define mm_info_about_test_t=
$(eval $(1).name:=)
$(eval $(1).filepathPerExecutable:=)
$(eval $(1).scripts:=)
endef

MM_INFO_PER_TEST:=

#*********************************** checks ***********************************

# NOTE: $(1) == functionname
#       $(2) == <mm_add_test_parameters_t>
define mm_check_add_test_parameters_t=
$(call mm_check_if_defined,$(1),$(2).executables)
$(call mm_check_if_defined,$(1),$(2).scripts)
endef

#******************************************************************************

# NOTE: $(1) == testname
mm_is_test=$(call mm_is_resource,TEST,$(1))

# NOTE: $(1) == testname
#       $(2) == <mm_add_test_parameters_t>
define mm_add_test=
$(if $(filter undefined,$(origin MM_SAFETY)),,\
	$(if $(filter 1,$(call mm_is_test,$(1))),$(error attempted to add test $(1) more than once in $(0)),)\
	$(call mm_check_add_test_parameters_t,$(0),$(2))\
)
$(eval MM_INFO_PER_TEST+=MM_INFO_PER_TEST.$(words $(MM_INFO_PER_TEST)))
$(eval $(0)_infoAboutTest:=$(lastword $(MM_INFO_PER_TEST)))
$(eval $($(0)_infoAboutTest).name:=$(1))
$(foreach $(0)_executable,$($(2).executables),\
	$(foreach $(0)_infoAboutExecutable,$(MM_INFO_PER_EXECUTABLE),\
		$(if $(filter $($(0)_executable),$($($(0)_infoAboutExecutable).name)),\
			$(eval $($(0)_infoAboutTest).filepathPerExecutable+=$($($(0)_infoAboutExecutable).$(MM_OS)$(MM_EXECUTABLE_EXTENSION_OR_DOT)filepath))\
		,)\
	)\
)
$(eval $($(0)_infoAboutTest).scripts:=$($(2).scripts))
endef

#******************************************************************************
#                                     stop
#******************************************************************************

MM_RELEASE:=

MM_RELEASEBINARIES:=
MM_RELEASEZIPBINARIES:=
MM_RELEASEINSTALLERBINARIES:=

MM_RELEASEFILES:=
MM_RELEASEZIPFILES:=
MM_RELEASEINSTALLERFILES:=

# NOTE: EMMReleasetype_Zip -> .<windows/linux>.zip
#       EMMReleasetype_Installer -> if windows.. .msi
#                                   if linux.. .deb and .snap
# NOTE: make release <(zip|installer)>
# NOTE: no "EMMReleasetype_Source" as source release == git main branch + tag
EMMReleasetype:=EMMReleasetype_Zip EMMReleasetype_Installer
EMMReleasetype_All:=$(EMMReleasetype)

# NOTE: $(1) == variablename
define mm_stop_parameters_t=
$(eval $(1).releasetypes:=)
$(eval $(1).ifRelease.additionalfiles:=)
$(eval $(1).ifRelease.ignoredbinaries:=)
$(eval $(1).ifRelease.ifZip.additionalfiles:=)
$(eval $(1).ifRelease.ifZip.ignoredbinaries:=)
$(eval $(1).ifRelease.ifInstaller.additionalfiles:=)
$(eval $(1).ifRelease.ifInstaller.ignoredbinaries:=)
endef
# NOTE: ^
#       .ifRelease.additionalfiles == empty or file(s) not made by..
#       .. makefile-mini to include in all releases
#       .ifRelease.ignoredbinaries == empty or binary/binaries to not..
#       .. include in all releases
#       .ifRelease.if*.additionalfiles == empty or file(s) not made by..
#       .. makefile-mini to include in corresponding release
#       .ifRelease.if*.ignoredbinaries == empty or binary/binaries to..
#       .. include in corresponding release

#*********************************** checks ***********************************

# NOTE: $(1) == functionname
#       $(2) == <mm_stop_parameters_t>
define mm_check_stop_parameters_t=
$(call mm_check_if_defined,$(1),$(2).releasetypes)
$(call mm_check_if_defined,$(1),$(2).ifRelease.additionalfiles)
$(call mm_check_if_defined,$(1),$(2).ifRelease.ignoredbinaries)
$(call mm_check_if_defined,$(1),$(2).ifRelease.ifZip.additionalfiles)
$(call mm_check_if_defined,$(1),$(2).ifRelease.ifZip.ignoredbinaries)
$(call mm_check_if_defined,$(1),$(2).ifRelease.ifInstaller.additionalfiles)
$(call mm_check_if_defined,$(1),$(2).ifRelease.ifInstaller,ignoredbinaries)

$(call mm_check_if_valid_values,$(1),$(EMMReleasetype_All),$(2).releasetypes)
endef

#******************************************************************************

# NOTE: $(1) == infoAboutSpvasmFromGlsl
define mm_add_spvasm_from_glsl_target=
$(eval $(0)_a:=$(strip $(call mm_switch,$($(1).type),EMMShadertype_Vertex EMMShadertype_Pixel,vert frag)))
.makefile-mini/$($(1).spvasm):.makefile-mini/%.spvasm:%.glsl | $(call mm_get_binarypartfolderpath_from_binarypart,$($(1).spvasm))
	glslangValidator $($(1).glslangValidator) --quiet -o $(MM_CLI_DEV_NULL) --spirv-dis -V -S $($(0)_a) $$< > $$@
endef
# NOTE: ^
#       -o $(MM_CLI_DEV_NULL) is to work around glslangValidator always..
#       .. outputting a file, ..
#       .. https://github.com/KhronosGroup/glslang/issues/3368
#       --quiet as otherwise first line glslangValidator outputs is path to..
#       .. inputfile

# NOTE: $(1) == infoAboutShader
define mm_add_spv_from_spvasm_target=
$($(1).spvFilepath): .makefile-mini/$($(1).spvasm) $(if $(patsubst .makefile-mini/%,%,$($(1).spvFilepath)),| $(call mm_get_binarypartfolderpath_from_binarypart,$($(1).spv)),)
	spirv-as -o $$@ $$<
endef
# NOTE: ^
#       if $(1).spvFilepath starts with .makefile-mini/.. this .spv might be..
#       .. a binarypart (though could also be an ignored binary)
#       otherwise.. this .spv is a binary

# NOTE: $(1) == infoAboutShader
define mm_add_spv_h_from_spv_target=
$($(1).spvHFilepath): $($(1).spvFilepath)
	$(call mm_cli_hfile_from_file,$($(1).name)_spv,$$<,$$@)
endef

# NOTE: $(1) == infoAboutShader
define mm_add_shader_target=
$(call mm_add_spv_from_spvasm_target,$(1))
$(if $($(1).spvHFilepath),$(call mm_add_spv_h_from_spv_target,$(1)),)
endef
# NOTE: ^
#       $(1).spvFilepath is guaranteed to be not empty

# NOTE: $(1) == infoAboutOFromC
define mm_add_o_from_c_target=
$(if $(OS),.makefile-mini/$($(1).o):.makefile-mini/%.o:%,.makefile-mini/$($(1).o):$($(1).c)) | $(call mm_get_binarypartfolderpath_from_binarypart,$($(1).o))
	gcc $($(1).gcc) -o $$@ -c $$< $(addprefix -I,$($(1).hFolders))
endef

# NOTE: $(1) == infoAboutOFromCpp
define mm_add_o_from_cpp_target=
$(if $(OS),.makefile-mini/$($(1).o):.makefile-mini/%.o:%,.makefile-mini/$($(1).o):$($(1).cpp)) | $(call mm_get_binarypartfolderpath_from_binarypart,$($(1).o))
	g++ $($(1).g++) -o $$@ -c $$< $(addprefix -I,$($(1).hppFolders))
endef
# NOTE: ^
#       see mm_add_o_from_c_target

# NOTE: $(1) == infoAboutLibrary
define mm_add_local_staticlibrary_targets=
$(eval $(0)_filepathPerLocalStaticO:=$(addprefix .makefile-mini/,$($(1).localStaticO)))
.makefile-mini/$($(1).$(MM_OS)$(MM_STATICLIBRARY_EXTENSION)filepath).nm:$($(0)_filepathPerLocalStaticO)
	nm -j -g --defined-only $$^ > $$@

$(eval $(0)_filepathPerStaticO:=$(addprefix .makefile-mini/,$($(1).staticO)))
.makefile-mini/$($(1).$(MM_OS)$(MM_STATICLIBRARY_EXTENSION)filepath).o:.makefile-mini/$($(1).$(MM_OS)$(MM_STATICLIBRARY_EXTENSION)filepath).nm $($(0)_filepathPerStaticO)
	ld -r -o $$@ $($(0)_filepathPerStaticO)
	objcopy --localize-symbols $$< $$@

$($(1).$(MM_OS)$(MM_STATICLIBRARY_EXTENSION)filepath): .makefile-mini/$($(1).$(MM_OS)$(MM_STATICLIBRARY_EXTENSION)filepath).o $($(1).hAndHppFilepathPerOtherLibrary)
	ar rcs $$@ $$<
endef
# NOTE: ^
#       https://stackoverflow.com/a/2980126
#       https://stackoverflow.com/a/44674115
#       ^
#       not using objcopy --local-hidden, but result is the same

# NOTE: $(1) == infoAboutLibrary
define mm_add_staticlibrary_target=
$(eval $(0)_staticO:=$(addprefix .makefile-mini/,$($(1).staticO)))
$($(1).$(MM_OS)$(MM_STATICLIBRARY_EXTENSION)filepath): $($(0)_staticO) $($(1).hAndHppFilepathPerOtherLibrary)
	ar rcs $$@ $($(0)_staticO)
endef

# NOTE: $(1) == infoAboutLibrary
define mm_add_staticlibrary_targets=
$(if $($(1).localStaticO),$(call mm_add_local_staticlibrary_targets,$(1)),$(call mm_add_staticlibrary_target,$(1)))
endef

# NOTE: $(1) == infoAboutLibrary
define mm_add_sharedlibrary_target=
$(eval $(0)_filepathPerSharedO:=$(addprefix .makefile-mini/,$($(1).sharedO)))
$($(1).$(MM_OS)$(MM_SHAREDLIBRARY_EXTENSION)filepath): $($(0)_filepathPerSharedO) $($(1).hAndHppFilepathPerOtherLibrary)
	$($(1).cc) -shared -o $$@ $($(0)_filepathPerSharedO)
endef

# NOTE: $(1) == infoAboutLibrary
define mm_add_library_targets=
$(if $(filter EMMLibraryfiletype_Static,$($(1).filetypes)),$(call mm_add_staticlibrary_targets,$(1)),)
$(if $(filter EMMLibraryfiletype_Shared,$($(1).filetypes)),$(call mm_add_sharedlibrary_target,$(1)),)	
endef

ifndef OS #< linux
# NOTE: $(1) == infoAboutExecutable
define mm_add_appimage_target=
endef
endif

# NOTE: $(1) == infoAboutExecutable
define mm_add_executable_targets=
$(eval $(0)_filepathPerO:=$(addprefix .makefile-mini/,$($(1).o)))
$($(1).$(MM_OS)$(MM_EXECUTABLE_EXTENSION_OR_DOT)filepath): $($(0)_filepathPerO) $($(1).hAndHppFilepathPerLibrary) $($(1).binaryfilepathPerStaticlibrary) $($(1).binaryfilepathPerSharedlibrary)
	$($(1).cc) $($(1).gccOrG++) -o $$@ $($(0)_filepathPerO) $(addprefix -L,$($(1).libFolders)) $(addprefix -l,$($(1).lib))
endef
# TODO: ^
#       .h prerequisites should be order only such that $$^ here still  works

#$(if $(OS),,$\
#$(if $($(1).linux.appimagefilepath),$(call mm_add_appimage_target))$\
#)

define mm_add_default_target=
.PHONY: default
default: $(MM_NOTIGNOREDBINARIES)
endef

# NOTE: $(1) == folderpath
define mm_add_folder_target=
$(1):
	$(call mm_cli_mkdir,$(1))
endef

define mm_add_folders_targets=
$(foreach $(0)_binarypartfolderpathpart,$(MM_FOLDERPATHPART_PER_BINARYPART),$(call mm_add_folder_target,.makefile-mini/$($(0)_binarypartfolderpathpart)))
endef
# NOTE: ^
#       "subst /,\" because mm_cli_mkdir parameter $(1) is non cli

define mm_add_test_target=
.PHONY: test
test: $(MM_FILEPATH_PER_BINARY)
	$(foreach $(0)_infoAboutTest,$(MM_INFO_PER_TEST),$\
	$(foreach $(0)_executablefilepath,$($($(0)_infoAboutTest).filepathPerExecutable),$\
	$(eval $(0)_a:=$(if $(findstring /,$($(0)_executablefilepath)),,.$(MM_FOLDER_SEPARATOR))$($(0)_executablefilepath))$\
	$(eval $(0)_b:=@export LD_LIBRARY_PATH=$$$$LD_LIBRARY_PATH:./:.makefile-mini/; $($(0)_a); echo $($(0)_a))$\
	$(MM_NEWLINE)	$(if $(OS),$($(0)_a),$($(0)_b))$\
	)$\
	$(foreach $(0)_script,$($($(0)_infoAboutTest).scripts),$\
	$(MM_NEWLINE)	.$(MM_FOLDER_SEPARATOR)$($(0)_script)$(MM_SCRIPT_EXTENSION)$\
	)$\
	)
endef
# NOTE: ^
#       I tried..
#       cmd /c "start /b $($(0)_script).bat"
#       .. which caused powershell to keep cmd.exe open after make returned..
#       .. thus having manually call exit,..
#       .. https://stackoverflow.com/a/41411671
#       Start-Job -ScriptBlock { Set-Location $using:pwd; Invoke-Expression..
#       .. .\$args } -ArgumentList "$($(0)_script).bat"
#       .. was the closest to working solution I found, but it broke the..
#       .. timeout command that I used to test whether scripts where ran in..
#       .. parallel, https://stackoverflow.com/a/74843981
#       ^
#       Hence for now.. executables and scripts are not run in parallel and..
#       .. .executables and .scripts is only for grouping tests (for..
#       .. convenience perhaps?)
# NOTE: ^
#       make automatically adds environment variables as Makefile variables..
#       .. (hence $(LD_LIBRARY_PATH) works) and runs each line using a new..
#       .. shell (separate c stdlib system function call?), hence export..
#       .. LD_LIBRARY_PATH=<..> on same line as running executable here
#       currently hidden that makefile-mini does this for clearer output
# TODO: this solution won't work if a test executable uses any external..
#       .. sharedlibrary/runs any executable that requires an external..
#       .. sharedlibrary

define mm_add_releasezip_target=
$(MM_PROJECTNAME).$(MM_OS).zip: $(MM_RELEASEZIP)
	$(call mm_cli_zip,$$@,$(MM_RELEASEZIP))
endef

define mm_add_releaseinstallermsi_target=
$(MM_PROJECTNAME).msi: $(MM_RELEASEINSTALLER)
endef
define mm_add_releaseinstallerdeb_target=
$(MM_PROJECTNAME).deb: $(MM_RELEASEINSTALLER)
endef
define mm_add_releaseinstallersnap_target=
$(MM_PROJECTNAME).snap: $(MM_RELEASEINSTALLER)
endef

define mm_add_releaseinstaller_targets=
$(if $(OS),\
$(call mm_add_releaseinstaller_msi_target),\
$(call mm_add_releaseinstaller_deb_target)\
$(call mm_add_releaseinstaller_snap_target)\
)
endef

define mm_add_release_targets=
$(if $(MM_RELEASEZIP),$(call mm_add_releasezip_target),)
$(if $(MM_RELEASEINSTALLER),$(call mm_add_releaseinstaller_targets),)

.PHONY: release
release: $(MM_RELEASE)
	@echo Reminder.. did you run "make test" before running "make release"?
endef

define mm_add_clean_target=
.PHONY: clean
clean:
	$(foreach $(0)_binarypart,$(MM_BINARYPARTS),$(MM_NEWLINE)	$(call mm_cli_rm,.makefile-mini/$($(0)_binarypart)))
	$(foreach $(0)_binarypartfolderpathpart,$(MM_FOLDERPATHPART_PER_BINARYPART),$(MM_NEWLINE)	$(call mm_cli_rmdir,.makefile-mini/$($(0)_binarypartfolderpathpart)))
	$(foreach $(0)_binaryfilepath,$(MM_FILEPATH_PER_BINARY),$(MM_NEWLINE)	$(call mm_cli_rm,$($(0)_binaryfilepath)))
	$(foreach $(0)_release,$(MM_RELEASE),$(MM_NEWLINE)	$(call mm_cli_rm,$($(0)_release)))
endef
# NOTE: ^
#       $(MM_NEWLINE)<tab>$(call <...>)
#                    ^
#                    to assure ends up in clean target?
# NOTE: ^
#       if windows.. using "/S /Q" in mm_cli_rmdir as order in which..
#       .. deleting folders can mean that a folder attempted to be deleted..
#       .. can contain folder(s)
#       ^
#       sort cannot "be used instead" as would result in reverse order?

# NOTE: $(1) == <mm_stop_parameters_t>
define mm_stop=
$(if $(filter undefined,$(origin MM_SAFETY)),,\
	$(call mm_check_add_makefile_parameters_t,$(0),$(1))\
)

$(eval $(call mm_add_default_target))

$(eval $(call mm_add_folders_targets))

$(foreach $(0)_infoAboutSpvasmFromGlsl,$(MM_INFO_PER_SPVASM_FROM_GLSL),$\
$(eval $(call mm_add_spvasm_from_glsl_target,$($(0)_infoAboutSpvasmFromGlsl)))$\
)

$(foreach $(0)_infoAboutShader,$(MM_INFO_PER_SHADER),$\
$(eval $(call mm_add_shader_target,$($(0)_infoAboutShader)))$\
)

$(foreach $(0)_infoAboutOFromC,$(MM_INFO_PER_O_FROM_C),$\
$(eval $(call mm_add_o_from_c_target,$($(0)_infoAboutOFromC)))$\
)

$(foreach $(0)_infoAboutOFromCpp,$(MM_INFO_PER_O_FROM_CPP),$\
$(eval $(call mm_add_o_from_cpp_target,$($(0)_infoAboutOFromCpp)))$\
)

$(foreach $(0)_infoAboutLibrary,$(MM_INFO_PER_LIBRARY),$\
$(eval $(call mm_add_library_targets,$($(0)_infoAboutLibrary)))$\
)

$(foreach $(0)_infoAboutExecutable,$(MM_INFO_PER_EXECUTABLE),$\
$(eval $(call mm_add_executable_targets,$($(0)_infoAboutExecutable)))$\
)

$(eval $(call mm_add_test_target))

$(if $($(1).releasetypes),\
	$(eval MM_RELEASEBINARIES:=$(if $($(1).ifRelease.ignoredbinaries),$(call mm_filter_out_using_patterns,$($(1).ifRelease.ignoredbinaries),$(MM_NOTIGNOREDBINARIES)),$(MM_NOTIGNOREDBINARIES)))\
	$(eval MM_RELEASEFILES_PATTERNS:=$$($(1).ifRelease.additionalfiles))\
	$(if $(filter EMMReleasetype_Zip,$($(1).releasetypes)),\
		$(eval MM_RELEASEFILES_PATTERNS+=$(if $(MM_RELEASEFILES_PATTERNS), ,)$$($(1).ifRelease.ifZip.additionalfiles))\
	)\
	$(if $(filter EMMReleasetype_Installer,$($(1).releasetypes)),\
		$(eval MM_RELEASEFILES_PATTERNS+=$(if $(MM_RELEASEFILES_PATTERNS), ,)$$($(1).ifRelease.ifInstaller.additionalfiles))\
	)\
	$(eval MM_RELEASEFILES:=$(if $(MM_RELEASEFILES_PATTERNS),$(call mm_filter_using_patterns,$(MM_RELEASEFILES_PATTERNS),$(filter-out .makefile-mini/%,$(call mm_get_path_to_file_per_file))),))\
	$(if $(filter EMMReleasetype_Zip,$($(1).releasetypes)),\
		$(eval MM_RELEASEZIPBINARIES:=$(if $($(1).ifRelease.ifZip.ignoredbinaries),$(call mm_filter_out_using_patterns,$($(1).ifRelease.ifZip.ignoredbinaries),$(MM_RELEASEBINARIES)),$(MM_RELEASEBINARIES)))\
		$(eval MM_RELEASEZIPFILES:=$(if $($(1).ifRelease.ifZip.additionalfiles),$(call mm_filter_using_patterns,$($(1).ifRelease.ifZip.additionalfiles),$(MM_RELEASEFILES)),))\
		$(eval MM_RELEASEZIP:=$(MM_RELEASEZIPBINARIES) $(MM_RELEASEZIPFILES))\
		$(if $(MM_RELEASEZIP),\
			$(eval MM_RELEASE+=$(MM_PROJECTNAME).$(MM_OS).zip),\
			$(if $(filter undefined,$(origin MM_SAFETY)),,\
				$(info warning: release $(MM_PROJECTNAME).$(MM_OS).zip is cancelled as no files specified)\
			)\
		)\
	,)\
	$(if $(filter EMMReleasetype_Installer,$($(1).releasetypes)),\
		$(eval MM_RELEASEINSTALLERBINARIES:=$(if $($(1).ignoredbinaries),$(call mm_filter_out_using_patterns,$($(1).ifRelease.ifInstaller.ignoredbinaries),$(MM_RELEASEBINARIES)),$(MM_RELEASEBINARIES)))\
		$(eval MM_RELEASEINSTALLERFILES:=$(if $($(1).additionalfiles),$(call mm_filter_using_patterns,$($(1).ifRelease.ifInstaller.additionalfiles),$(MM_RELEASEFILES)),))\
		$(eval MM_RELEASEINSTALLER:=$(MM_RELEASEINSTALLERBINARIES) $(MM_RELEASEINSTALLERFILES))\
		$(if $(MM_RELEASEINSTALLER),\
			$(eval MM_RELEASE+=$(addprefix $(MM_PROJECTNAME),$(MM_RELEASEINSTALLER_EXTENSIONS))),\
			$(if $(filter undefined,$(origin MM_SAFETY)),,\
				$(info warning: release file(s) $(addprefix $(MM_PROJECTNAME),$(MM_RELEASEINSTALLER_EXTENSIONS)) is/are cancelled as no files specified)\
			)\
		)\
	,)\
,)
$(if $(MM_RELEASE),\
$(eval $(call mm_add_release_targets))\
,)

$(eval $(call mm_add_clean_target))
endef
# NOTE: ^
#       strip in MM_RELEASEFILES_PATTERNS:=$(strip <..>) because $(if..
#       .. $(MM_RELEASEFILES_PATTERNS),<..>)
# NOTE: ^
#       $$ in $(eval MM_RELEASEFILES_PATTERNS:=$$<..>) and $(eval..
#       .. MM_RELEASEFILES_PATTERNS:=<..>$$<..>) because patterns..
#       .. may contain $ which should not be directly supplied to eval
# NOTE: ^
#       $(if $(MM_RELEASE*),,$(eval MM_RELEASE:=$(filter-out <..>,$(MM_RELEASE))))
#       ^
#       if releasetype specified but no files.. don't make release
