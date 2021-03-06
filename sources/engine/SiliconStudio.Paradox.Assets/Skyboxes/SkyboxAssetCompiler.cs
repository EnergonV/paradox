﻿// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.

using System.Collections.Generic;
using System.Threading.Tasks;

using SiliconStudio.Assets.Compiler;
using SiliconStudio.BuildEngine;
using SiliconStudio.Core;
using SiliconStudio.Core.IO;
using SiliconStudio.Core.Serialization;
using SiliconStudio.Core.Serialization.Assets;
using SiliconStudio.Paradox.Assets.Textures;
using SiliconStudio.Paradox.Graphics;

namespace SiliconStudio.Paradox.Assets.Skyboxes
{
    internal class SkyboxAssetCompiler : AssetCompilerBase<SkyboxAsset>
    {
        protected override void Compile(AssetCompilerContext context, string urlInStorage, UFile assetAbsolutePath, SkyboxAsset asset, AssetCompilerResult result)
        {
            result.BuildSteps = new AssetBuildStep(AssetItem);
            result.ShouldWaitForPreviousBuilds = true;

            // build the textures for windows (needed for skybox compilation)
            foreach (var dependency in asset.Model.GetDependencies())
            {
                var assetItem = context.Package.Assets.Find(dependency.Id);
                if (assetItem != null && assetItem.Asset is TextureAsset)
                {
                    var textureAsset = (TextureAsset)assetItem.Asset;

                    // Get absolute path of asset source on disk
                    var assetSource = GetAbsolutePath(assetItem.Location.GetDirectoryAndFileName(), textureAsset.Source);

                    // Create a synthetic url
                    var textureUrl = SkyboxGenerator.BuildTextureForSkyboxGenerationLocation(assetItem.Location);

                    var gameSettingsAsset = context.GetGameSettingsAsset();

                    // Create and add the texture command.
                    var textureParameters = new TextureConvertParameters(assetSource, textureAsset, PlatformType.Windows, GraphicsPlatform.Direct3D11, GraphicsProfile.Level_10_0, gameSettingsAsset.TextureQuality);
                    result.BuildSteps.Add(new AssetBuildStep(AssetItem) { new TextureAssetCompiler.TextureConvertCommand(textureUrl, textureParameters) });
                }
            }

            // add the skybox command itself.
            result.BuildSteps.Add(new WaitBuildStep());
            result.BuildSteps.Add(new SkyboxCompileCommand(urlInStorage, asset));
        }

        private class SkyboxCompileCommand : AssetCommand<SkyboxAsset>
        {
            public SkyboxCompileCommand(string url, SkyboxAsset assetParameters)
                : base(url, assetParameters)
            {
            }

            /// <inheritdoc/>
            protected override void ComputeParameterHash(BinarySerializationWriter writer)
            {
                base.ComputeParameterHash(writer);
                writer.Write(1); // Change this number to recompute the hash when prefiltering algorithm are changed
            }

            /// <inheritdoc/>
            public override IEnumerable<ObjectUrl> GetInputFiles()
            {
                if (AssetParameters.Model != null)
                {
                    foreach (var dependency in AssetParameters.Model.GetDependencies())
                    {
                        yield return new ObjectUrl(UrlType.Internal, dependency.Location);
                    }
                }
            }

            /// <inheritdoc/>
            protected override Task<ResultStatus> DoCommandOverride(ICommandContext commandContext)
            {
                // TODO Convert SkyboxAsset to Skybox and save to Skybox object
                // TODO Add system to prefilter

                using (var context = new SkyboxGeneratorContext())
                {
                    var result = SkyboxGenerator.Compile(AssetParameters, context);

                    if (result.HasErrors)
                    {
                        result.CopyTo(commandContext.Logger);
                        return Task.FromResult(ResultStatus.Failed);
                    }

                    context.Assets.Save(Url, result.Skybox);
                }

                return Task.FromResult(ResultStatus.Successful);
            }
        }
    }
}
 
