#!/usr/bin/awk -f 

BEGIN {
	in_function=0;
	function_name="";
}

/[_a-z]+[[:space:]]*(.*)[[:space:]]*{[[:space:]]*C[[:space:]]*Descriptor/ {
	#name=$0;
	#sub("\\(.*$", "", name);
	#print "function name: ", name;

	indented=0;
	line=$0;
	indented += gsub("{", "{", line);
	indented -= gsub("}", "}", line);
	sub("[[:space:]]*C[[:space:]]*Descriptor$", "");
	sub("^[[:space:]]+", "");
	gsub("[[:space:]]+", " ");

	print $0;

	indent_tmp=0;
	while (getline)
	{
		# remove initial space
		sub("^[[:space:]]+", "");

		# remove page seperator
		sub("--.+---$", "");
		if ($0 == "© ISO/IEC 2004 – All rights reserved")
		{
			do
			{
				if (!getline)
					break;
				sub("^[[:space:]]+", "");
				#print $0;
			}
			while ($0 != "ISO/IEC 14496-10:2004(E)");
			continue;
		}
		if (sub("^Copyright International Organization", ""))
		{
			do
			{
				if (!getline)
					break;
				sub("^[[:space:]]+", "");
				#print $0;
			}
			while ($0 != "Reproduced by IHS under license with ISO" &&
			       $0 != "ISO/IEC 14496-10:2004(E)");
			continue;
		}

		# remove empty line
		if ($0 == "")
			continue;

		# remove single line comment
		gsub("\\/\\*.*\\*\\/", "");

		# reduce space
		gsub("[[:space:]]+", " ");

		gsub("\\[ ", "[");
		gsub(" \\]", "]");
		gsub("\\( ", "(");
		gsub(" \\)", ")");
		gsub(", ", ",");
		gsub(" \\| ", "|");
		gsub(" \\+ ", "+");
		gsub(" \\* ", "*");
		gsub("\\| \\|", "||");
		gsub("= =", "==");
		gsub("–", "-");

		line=$0;

		# bits description
		# in vim,
		# /[_a-zA-Z0-9]\+\(\[.\+\]\)*\( \(All\|\d\(|\d\)*\)\)\? \(ae\|b\|ce\|f\|i\|me\|se\|te\|u\|ue\)(\(\d\+\|v\))\(|\(ae\|b\|ce\|f\|i\|me\|se\|te\|u\|ue\)(\(\d\+\|v\))\)*

		# function call with category
		# /[_a-zA-Z0-9]\+(\([_a-zA-Z0-9]\+\(\[[^\]]\+\]\)*\(,[_a-zA-z0-9]\+\(\[[^\]]\+\]\)*\)*\)\?) \(All\|\d\(|\d\)*\)

		tmp=line;
		bits_desc = sub("^[_a-zA-Z0-9]+(\\[[^\\]]+\\])*( (All|([[:digit:]](\\|[[:digit:]])*)))? (ae|b|ce|f|i|me|se|te|u|ue)\\(([[:digit:]]+|v)\\)(\\|(ae|b|ce|f|i|me|se|te|u|ue)\\(([[:digit:]]+|v)\\))*", "", tmp);

		func_c = sub("^[_a-zA-Z0-9]+\\(([_a-zA-Z0-9]+(\\[[^\\]]+\\])*(,[_a-zA-z0-9]+(\\[[^\\]]+\\])*)*)?\\) (All|[[:digit:]](|[[:digit:]])*)", "", tmp);

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
			sub("^[_a-zA-Z0-9]+(\\[[^\\]]+\\])*", "&,", desc);
			if (sub(" (All|([[:digit:]](\\|[[:digit:]])*))", "&,", desc) == 0)
				# without category
				sub(", ", ", All, ", desc);

			# with category
			printf "h264_bitsdesc (";
			printf desc;
			printf ")\n";
		}
		else if (func_c > 0)
		{
			desc=line;
			sub("^[_a-zA-Z0-9]+\\(([_a-zA-Z0-9]+(\\[[^\\]]+\\])*(,[_a-zA-z0-9]+(\\[[^\\]]+\\])*)*)?\\)", "&,", desc);
			printf "h264_func_cat (";
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
		    sub("\\<if\\>|\\<else\\>|\\<while\\>|\\<for\\>", "", line) > 0)
			indent_tmp=1;
	}

	print "";
}
