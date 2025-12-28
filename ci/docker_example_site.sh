set -e

pushd .
echo '================================================================================'
echo 'Build example site in Docker'
echo '================================================================================'
cd ..
DOCKER_BUILDKIT=1 docker build -f examples/site/Dockerfile -t example-site .
popd