﻿// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.

namespace SiliconStudio.Paradox.Engine
{
    public abstract class StartupScript : Script
    {
        /// <summary>
        /// Called before the script enters it's update loop.
        /// </summary>
        public virtual void Start()
        {
        }

        /// <summary>
        /// Called when the script's update loop is canceled.
        /// </summary>
        public virtual void Cancel()
        {
        }
    }
}
