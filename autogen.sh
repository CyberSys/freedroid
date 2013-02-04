#! /bin/sh
if (aclocal -I m4); then
	if (autoheader); then
		if (automake --add-missing); then
			if (autoconf); then
				echo "autogen.sh ran successfully. Execute ./configure to proceed."
			else
				echo "Something failed, please make sure you have autoconf installed."
			fi
		else
			echo "Something failed, please make sure you have automake installed."
		fi
	else
		echo "Something failed, please make sure you have autotools installed."
	fi
else
	echo "Something failed, please make sure you have automake installed."
fi
