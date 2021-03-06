﻿// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.
using System;
using System.Collections.Generic;

using SiliconStudio.Core.IO;

namespace SiliconStudio.Assets
{
    /// <summary>
    /// Imports a raw asset into the asset system.
    /// </summary>
    public interface IAssetImporter
    {
        /// <summary>
        /// Gets an unique identifier to identify the importer. See remarks.
        /// </summary>
        /// <value>The identifier.</value>
        /// <remarks>This identifier is used to recover the importer used for a particular asset. This 
        /// Guid must be unique and stored statically in the definition of an importer. It is used to 
        /// reimport an existing asset with the same importer.
        /// </remarks>
        Guid Id { get; }

        /// <summary>
        /// Gets the name of this importer.
        /// </summary>
        /// <value>The name.</value>
        string Name { get; }

        /// <summary>
        /// Gets the description of this importer.
        /// </summary>
        /// <value>The description.</value>
        string Description { get; }

        /// <summary>
        /// Gets the order of precedence between the importers, so that an importer can override another one.
        /// </summary>
        /// <value>The order.</value>
        int Order { get; }

        /// <summary>
        /// Gets the supported file extensions (separated by ',' for multiple extensions) by this importer. This is used for display purpose only. The method <see cref="IsSupportingFile"/> is used for matching extensions.
        /// </summary>
        /// <returns>Returns a list of supported file extensions handled by this importer.</returns>
        string SupportedFileExtensions { get; }

        /// <summary>
        /// Determines whether this importer is supporting the specified file.
        /// </summary>
        /// <param name="filePath">The file path.</param>
        /// <returns><c>true</c> if this importer is supporting the specified file; otherwise, <c>false</c>.</returns>
        bool IsSupportingFile(string filePath);

        /// <summary>
        /// Gets the default parameters for this importer.
        /// </summary>
        /// <param name="isForReImport"></param>
        /// <value>The supported types.</value>
        AssetImporterParameters GetDefaultParameters(bool isForReImport);

        /// <summary>
        /// Imports a raw assets from the specified path into the specified package.
        /// </summary>
        /// <param name="rawAssetPath">The path to a raw asset on the disk.</param>
        /// <param name="importParameters">The parameters. It is mandatory to call <see cref="GetDefaultParameters"/> and pass the parameters instance here</param>
        IEnumerable<AssetItem> Import(UFile rawAssetPath, AssetImporterParameters importParameters);
    }
}