set -e

pushd .

cd ../tests
mkdir -p build
cd build


if [[ $OSTYPE == 'darwin'* ]]
then 
	# Copy vendor-provided MySQL Connector/OpenSSL libs into local build dir
	cp -r ../../src/mysqlconnector/lib64/*.* .

	# On macOS, the bundled libssl.3.dylib may reference libcrypto.3.dylib
	# via an absolute MySQL path (/usr/local/mysql/lib/libcrypto.3.dylib),
	# which is not present on CI runners. Rewrite it to load from this folder.
	if [[ -f libssl.3.dylib && -f libcrypto.3.dylib ]]; then
		install_name_tool -change \
			"/usr/local/mysql/lib/libcrypto.3.dylib" \
			"@loader_path/libcrypto.3.dylib" \
			libssl.3.dylib || true
	fi

	# Ensure dyld can find local libs during test run
	export DYLD_LIBRARY_PATH="$(pwd):${DYLD_LIBRARY_PATH}"
fi

cmake ..
make -j${nproc}

echo ''
echo '================================================================================'
echo 'Unit tests:'
echo '================================================================================'
./server_unit_tests

echo ''
echo '================================================================================'
echo 'Integration tests:'
echo '================================================================================'
./server_integration_tests
