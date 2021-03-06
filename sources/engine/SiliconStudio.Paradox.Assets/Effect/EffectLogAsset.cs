﻿// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.

using System.IO;
using System.Text;

using SiliconStudio.Assets;
using SiliconStudio.Assets.Compiler;
using SiliconStudio.Core;


namespace SiliconStudio.Paradox.Assets.Effect
{
    /// <summary>
    /// Describes an effect asset. 
    /// </summary>
    [DataContract("EffectLibrary")]
    [AssetDescription(FileExtension, false)]
    [AssetCompiler(typeof(EffectLogAssetCompiler))]
    [Display(98, "Effect Library", "An effect library")]
    public sealed class EffectLogAsset : SourceCodeAsset
    {
        /// <summary>
        /// The default file extension used by the <see cref="EffectLogAsset"/>.
        /// </summary>
        public const string FileExtension = ".pdxeffectlog";

        /// <summary>
        /// Initializes a new instance of the <see cref="EffectLogAsset"/> class.
        /// </summary>
        public EffectLogAsset()
        {
        }

        /// <summary>
        /// Gets the text.
        /// </summary>
        /// <value>The text.</value>
        public string Text
        {
            get; set;
        }

        public override void Load()
        {
            if (!string.IsNullOrEmpty(AbsoluteSourceLocation))
            {
                Text = File.ReadAllText(AbsoluteSourceLocation);
            }
        }

        public override void Save(Stream stream)
        {
            if (Text != null)
            {
                var buffer = Encoding.UTF8.GetBytes(Text);
                stream.Write(buffer, 0, buffer.Length);
            }
        }

        protected override int InternalBuildOrder
        {
            get { return 100; }
        }
    }
}