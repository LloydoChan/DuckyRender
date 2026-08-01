現時点では、このリポジトリは大きく分けて2つのプロジェクトで構成されています。

1つ目は、DirectX 12でフルスクラッチ実装したシンプルなレンダラー 「DuckyRender」 です。このレンダラーは、同じリポジトリ内のもう1つのプロジェクトである 「DuckyDataCooker」 によって変換（Cook）されたモデルを描画します。

2つ目の DuckyDataCooker は、glTFファイルを読み込み、レンダラーで使用できる「Cook済み」データへ変換するツールです。

コードをビルドするには、まず DirectXTex をダウンロードし、インクルードディレクトリを適切に設定してください。

https://github.com/microsoft/DirectXTex

また、DuckyDataCooker を使用するためには、DirectXTexConv（Texconv） もダウンロードする必要があります。

https://github.com/microsoft/DirectXTex/wiki/Texconv

レンダラーを実行するには、以下のようなフォルダー構成にする必要があります。

DuckyDataCooker と DuckyRender を同じ親フォルダー内に配置します。
さらに、その親フォルダーに Assets フォルダーを作成し、その中に InputAssets と CookedAssets フォルダーを作成してください。

フォルダー構成は以下のようになります。

DuckyRender/
├─ DuckyDataCooker/
├─ DuckyRender/
├─ Assets/
│  ├─ InputAssets/
│  ├─ CookedAssets/

変換したいglTFファイルを InputAssets フォルダーへ配置し、以下のコマンドラインで DuckyDataCooker を実行してください。

DuckyDataCooker <ModelName>

変換後の .Ducky ファイルとテクスチャは、CookedAssets フォルダー内の対応するフォルダーへ出力されます。

その後、以下のコマンドラインでレンダラーを実行してください。

DuckyRender -input <ModelName>

正常に動作すれば、モデルが画面に描画されます。

問題が発生した場合は、DuckyRender.exe と同じディレクトリに出力される log.txt に詳細なエラーログが記録されます。

This, at the moment, is a project of two halves: one is a simple renderer named "DuckyRender" made in DX12 from scratch to render models that have been 
cooked/processed by the other project in this repo,  the DuckerCooker which takes gltf files and then creates a "cooked" version of the model for use in the renderer.

If you pull the code you will need to download DirectXTex: https://github.com/microsoft/DirectXTex and set your include directories appropriately
You will also need to download the DirectXTexConv texture converter tool for the DuckyCooker - available here: https://github.com/microsoft/DirectXTex/wiki/Texconv

If you want to get the actual renderer running there is a two step process: you will need to place the two DuckyDataCooker and DuckyRender Folders in the same folder, and create an 
"Assets" folder above them with "InputAssets" and "CookedAssets" folders.

Please look at the following diagram

DuckyRender/
├─ DuckyDataCooker/
├─ DuckyRender/
├─ Assets/
│  ├─ InputAssets/
│  ├─ CookedAssets/


Place the gltf files you want converted in the InputAssets folder, run the DuckyDataCooker with the command 
line
      "DuckyDataCooker <ModelName>"

and the processed .Ducky file will be output in a corresponding folder with textures in the CookedAssets folder

Then when running the renderer you will need to use the following command line 

"DuckyRender -input <ModelName>"

and the file should be rendered to screen, if there is a problem the issue will be logged to the log.txt file that will be written to in the same directory as the DuckyRender.exe file.
