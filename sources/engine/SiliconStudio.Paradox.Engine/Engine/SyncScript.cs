// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.
namespace SiliconStudio.Paradox.Engine
{
    /// <summary>
    /// A script whose <see cref="Update"/> will be called every frame.
    /// </summary>
    public abstract class SyncScript : StartupScript
    {
        /// <summary>
        /// Called every frame.
        /// </summary>
        public abstract void Update();
    }
}