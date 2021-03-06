﻿// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.

using System.ComponentModel;
using System.Globalization;

namespace SiliconStudio.Paradox.Rendering.Materials.ComputeColors
{
    /// <summary>
    /// An indexed stream. 
    /// </summary>
    public abstract class IndexedVertexStreamDefinition : VertexStreamDefinitionBase
    {
        protected IndexedVertexStreamDefinition()
        {
        }

        protected IndexedVertexStreamDefinition(int index)
        {
            Index = index;
        }

        /// <summary>
        /// Gets or sets the index.
        /// </summary>
        /// <value>The index.</value>
        /// <userdoc>The index in the named stream</userdoc>
        [DefaultValue(0)]
        public int Index { get; set; }

        protected abstract string GetSemanticPrefixName();

        public override string GetSemanticName()
        {
            return string.Format(CultureInfo.InvariantCulture, "{0}{1}", GetSemanticPrefixName(), Index);
        }
    }
}