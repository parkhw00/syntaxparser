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

	print $0;

	indent_tmp=0;
	while (getline)
	{
		# remove single line comment
		gsub("\\/\\*.*\\*\\/", "");
		gsub("‘", "\"");
		gsub("’", "\"");

		# some tricky
		gsub("sequence_scalable_extension() is present in the bitstream", "__stream_defined (\"sequence_scalable_extension\")");
		gsub("First DCT coefficient 2-24", "first_dct_coefficient 2-24 uimsbf");
		gsub("Subsequent DCT coefficients 3-24", "subsequent_dct_coefficient 3-24 uimsbf");
		gsub("End of block", "end_of_block");
		gsub("extensions_and_user_data", "extension_and_user_data");

		# do not parse macroblock()
		gsub("^macroblock\\(\\)$", "next_start_code()");

		line=$0;

		# search bit description
		# in vim,
		# /^\s*[a-zA-Z0-9_]\+\(\[\(\d\+\|[a-z]\)\]\)*\s\+\(\d\+\|\d\+ \?\(\*\|-\|or\) \?\d\+\)\s\+\(bslbf\|uimsbf\|simsbf\|vlclbf\|"[0-9 ]\+"\)


		tmp=line;
		bits_desc = sub("[a-zA-Z0-9_]+(\\[([[:digit:]]+|[a-z])\\])*[[:space:]]+([[:digit:]]+|[[:digit:]]+ ?(\\*|-|or) ?[[:digit:]]+)[[:space:]]+(bslbf|uimsbf|simsbf|vlclbf|\"[0-9 ]+\")", "", tmp);
		indent_minus = gsub("}", "}", line);
		indent_plus = gsub("{", "{", line);

		if (indent_plus>0)
			indent_tmp=0;

		indented -= indent_minus;

		for (a=0; a<indented+indent_tmp; a++)
			printf "\t";
		if (bits_desc > 0)
		{
			desc=line;
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
	}

	print "";
}
