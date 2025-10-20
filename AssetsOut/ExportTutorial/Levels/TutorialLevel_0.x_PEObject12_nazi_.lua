
-- this script is in lua format
-- this is a meta script that fills in data that is passed to 'myScript' that in turn calls C++ function
-- some of the data can be set by default, some of the data might be required to be edited from maya
function fillMetaInfoTable(args) -- the script fromat requires existance of this function

-- user modifed data
args['myScript']="Skin.lua"
args['myScriptPackage']="Default"

args['skelName'] = "vampire-t-pose_Hips.skela"
args['skelPackage'] = "Vampire"

args['meshName'] = "Vampire.mesha"
args['meshPackage'] = "Vampire"

args['animsetName'] = "vampire-action-adventure-pack_Hips.animseta"
args['animsetPackage'] = "Vampire"

args['defaultAnimSet'] = 0  
args['defaultAnim'] = 20

end -- required
