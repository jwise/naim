#!/bin/bash
echo "Installing OSCAR support into your home directory..."

set -x
mkdir -p ~/.naim
touch ~/.naim/userinit.lua
if ! grep OSCAR ~/.naim/userinit.lua >/dev/null ; then
	echo 'require"OSCAR"' >> ~/.naim/userinit.lua
fi
cp numutil.lua ~/.naim/numutil.lua
mkdir -p ~/.naim/OSCAR
cp init.lua ~/.naim/OSCAR/init.lua
cp FLAP.lua ~/.naim/OSCAR/FLAP.lua
cp SNAC.lua ~/.naim/OSCAR/SNAC.lua
cp TLV.lua ~/.naim/OSCAR/TLV.lua
set +x

echo "Looks done to me, unless you saw any errors above."