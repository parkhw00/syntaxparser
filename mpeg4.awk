#!/usr/bin/awk -f 

BEGIN {
	in_function=0;
	function_name="";
}

/[_a-z]+[[:space:]]*(.*)[[:space:]]*{[[:space:]]*No[[:space:]]*\. of bits Mnemonic/ {
	#name=$0;
	#sub("\\(.*$", "", name);
	#print "function name: ", name;

	indented=0;
	line=$0;
	indented += gsub("{", "{", line);
	indented -= gsub("}", "}", line);
	sub("[[:space:]]*No[[:space:]]*\\..*$", "");

	function_name = $0;
	sub ("[[:space:]]*\\(.*\\)[[:space:]]*\\{", "", function_name);

	#print "-", function_name, "-";
	print $0;

	indent_tmp=0;
	while (getline)
	{
		tmp = $0;

		# some tricky
		if (sub ("Licensed to ", ""))
		{
			do
			{
				if (!getline)
					break;
			}
			while (!sub ("© ISO/IEC 2001 – All rights reserved", ""));
			continue;
		}
		sub ("NOTE: Data partitioning is not supported in B-VOPs.", "}");
		sub ("NOTE: The value of block_count is 6 in the 4:2:0 format.*$", "}");
		sub ("6.2.9 Mesh Object", "}");
		sub ("if \\(is_i_new_min\\) {", "} &");
		sub ("if \\(is_p_new_max\\) {", "} &");
		sub ("if \\(is_p_new_min\\) {", "} &");
		sub ("if \\(bap_is_i_new_min\\) {", "} &");
		sub ("if \\(bap_is_p_new_max\\) {", "} &");
		sub ("if \\(bap_is_p_new_min\\) {", "} &");
		fallowing_ignore = sub ("creaseAngle 6 uimsb", "&f");
		sub ("if \\(normal_binding != “bound_per_face” &&", "} &");
		if (function_name == "VisualObject" && sub ("still texture$", "", tmp))
		{
			getline n;
			$0 = $0 " " n;
		}
		if (function_name == "VideoObjectLayer")
		{
			sub ("not_8_ bit", "not_8_bit");

			# some special case, mpeg4 *_quant_mat bit length
			sub ("8\\*\\[2-64\\]", "9999");
		}
		if (function_name == "root_color")
			sub ("else {", "} &");
		if (function_name == "root_texCoord")
			sub ("if \\(texCoord_binding!= “bound_per_vertex” \\|\\|", "} &");
		if (function_name == "triangle")
			sub ("if \\(triangulated == ‘0’\\)", "} &");
		if (function_name == "fs_post_update")
			sub ("}", "} &");
		if (function_name == "define_vop_complexity_estimation_header")
		{
			sub ("shape_complexity_estimation_disable 1", "& bslbf");
			sub ("{ bslbf", "{");
			sub ("texture_complexity_ estimation_set_2_disable", "texture_complexity_estimation_set_2_disable");
		}
		if (function_name == "VideoObjectPlane")
		{
			sub ("backward_shape_ height", "backward_shape_height");
			sub ("‘’binary only‘’", "\"binary only\"");
			sub ("“ binary only”", "\"binary only\"");
		}
		if (function_name == "video_plane_with_short_header")
			sub ("short_video _end_marker", "short_video_end_marker");
		if (function_name == "coord_header" || function_name == "color_header")
			sub ("'”parallelogram_prediction”", "\"parallelogram_prediction\"");
		if (function_name == "motion_vector")
		{
			sub ("„direct“", "\"direct\"");
			sub ("„forward“", "\"forward\"");
			sub ("„backward“", "\"backward\"");
		}
		if (function_name == "block")
			sub ("DCT coefficient", "DCT_coefficient");
		if (function_name == "alpha_block")
		{
			sub ("DCT coefficient", "DCT_coefficient");
			sub ("))))", ")))");
		}
		if (function_name == "StillTextureObject")
		{
			sub ("visual_object_verid != 0001", "visual_object_verid != \"0001\"");
			sub ("tiling jump_table_enable", "tiling_jump_table_enable");
		}
		if (function_name == "shape_object_decoding")
			sub ("visual_object_verid != 0001", "visual_object_verid != \"0001\"");

		# remove single line comment
		gsub("\\/\\*.*\\*\\/", "");
		gsub("‘", "\"");
		gsub("’", "\"");
		gsub("“", "\"");
		gsub("”", "\"");
		gsub("'", "\"");
		gsub("–", "-");

		line=$0;

		# search bit description
		# in vim,
		# /^\s*[a-zA-Z0-9_]\+\(\[\(\d\+\|[a-z]\)\]\)*\s\+\(\d\+\|\d\+ \?\(\*\|-\|or\) \?\d\+\)\s\+\(bslbf\|uimsbf\|simsbf\|vlclbf\|"[0-9 ]\+"\)

		bits_desc_pattern = "[a-zA-Z0-9_]+(\\[([[:digit:]]+|[a-z])\\])*[[:space:]]+([[:digit:]]+|[[:digit:]]+ ?(\\*|-|or) ?[[:digit:]]+)[[:space:]]+(bslbf|uimsbf|simsbf|vlclbf|\"[0-9 ]+\")";

		bits_desc_tmp=line;
		bits_desc = sub(bits_desc_pattern, "", bits_desc_tmp);
		indent_minus = gsub("}", "}", line);
		indent_plus = gsub("{", "{", line);

		if (indent_plus>0)
			indent_tmp=0;

		indented -= indent_minus;

		for (a=0; a<indented+indent_tmp; a++)
			printf "\t";
		if (bits_desc > 0)
		{
			printf bits_desc_tmp;

			desc=line;
			sub ("if \\(.*\\) ", "", desc);

			gsub("[[:space:]]+", " ", desc);
			sub("[a-zA-Z0-9_]+(\\[([[:digit:]]+|[a-z])\\])*", "&,", desc);
			sub("[[:space:]]+([[:digit:]]+|[[:digit:]]+ ?(\\*|-|or) ?[[:digit:]]+)", "&,", desc);
			printf "mpeg2_bitsdesc (";
			printf desc;
			printf ")\n";
		}
		else
			print $0;
		indent_tmp = 0;

		indented += indent_plus;

		if (indented <= 0)
			break;

		if (indent_plus == 0 &&
		    sub("\\<if\\>|\\<else\\>|\\<while\\>", "", line) > 0)
			indent_tmp=1;

		if (fallowing_ignore > 0)
			getline;
	}

	print "";
}
