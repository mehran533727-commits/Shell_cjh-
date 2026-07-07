#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────
# 一键配置脚本：组员 git pull 后运行一次，即可和作者一样使用
# （含离线依赖编译 + DeepSeek AI）。
#
#   用法：  bash cjh-setup.sh
#   环境：  Linux / WSL (Ubuntu 建议 22.04+)
# ─────────────────────────────────────────────────────────────
set -e
cd "$(dirname "$0")"
ROOT="$PWD"

echo "==> [1/4] 安装系统依赖（需要你机器的 sudo 密码）"
sudo apt update
sudo apt install -y cmake g++ make libcurl4-openssl-dev libsqlite3-dev nlohmann-json3-dev

echo "==> [2/4] 解压离线依赖（来自仓库内 vendor/*.tar，无需联网 GitHub）"
mkdir -p vendor
[ -d vendor/replxx-release-0.0.4 ] || tar -C vendor -xzf vendor/replxx-0.0.4.tar.gz
# googletest 仅在你想跑测试时需要（见文末）；一并解压，无害。
[ -f vendor/googletest-1.14.0.tar.gz ] && { [ -d vendor/googletest-1.14.0 ] || tar -C vendor -xzf vendor/googletest-1.14.0.tar.gz; }

echo "==> [3/4] 配置 DeepSeek（私密仓库，共用作者的 key）"
mkdir -p ~/.config/CJHSH && chmod 700 ~/.config/CJHSH
echo openai > ~/.config/CJHSH/ai_provider
printf '%s' 'sk-9cf7ec9045074048aed167c1c2323621' > ~/.config/CJHSH/openai_key
chmod 600 ~/.config/CJHSH/openai_key

echo "==> [4/4] 离线编译主程序"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
  -DFETCHCONTENT_SOURCE_DIR_REPLXX="$ROOT/vendor/replxx-release-0.0.4"
cmake --build build -j "$(getconf _NPROCESSORS_ONLN)"

echo ""
echo "✅ 完成！"
echo "   运行 shell：  ./build/CJHSH.out"
echo "   试试 AI：     进入后输入   @ai 查看隐藏文件用什么命令"
echo ""
echo "（可选）跑全部测试："
echo "   cmake -B build-test -DBUILD_TESTS=ON \\"
echo "     -DFETCHCONTENT_SOURCE_DIR_REPLXX=\"$ROOT/vendor/replxx-release-0.0.4\" \\"
echo "     -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=\"$ROOT/vendor/googletest-1.14.0\""
echo "   cmake --build build-test -j\$(nproc) && ctest --test-dir build-test -j\$(nproc)"
