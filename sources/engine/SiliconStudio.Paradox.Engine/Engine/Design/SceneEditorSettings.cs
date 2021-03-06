// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.

using SiliconStudio.Core;
using SiliconStudio.Core.Annotations;
using SiliconStudio.Core.Mathematics;

namespace SiliconStudio.Paradox.Engine.Design
{
    /// <summary>
    /// Settings for the editor when viewing a scene in the scene editor.
    /// </summary>
    [DataContract("SceneEditorSettings")]
    public sealed class SceneEditorSettings
    {
        // TODO: This class should contain more scene specific settings for the editor

        /// <summary>
        /// Initializes a new instance of the <see cref="SceneEditorSettings"/> class.
        /// </summary>
        public SceneEditorSettings()
        {
            GridColor = (Color3)new Color(180, 180, 180);
            SceneUnit = 1.0f;
            Camera = new SceneEditorCameraSettings();
            Mode = new SceneEditorGraphicsModeLDRSettings();
        }

        /// <summary>
        /// Gets or sets the color of the grid.
        /// </summary>
        /// <value>The color of the grid.</value>
        /// <userdoc>The color of the grid used in scene editor</userdoc>
        [DataMember(5)]
        public Color3 GridColor { get; set; }

        /// <summary>
        /// Gets or sets the scene unit, used to scale entity gizmos and camera speed.
        /// </summary>
        /// <value>The color of the background.</value>
        /// <userdoc>The scene unit, used to scale gizmos and camera speed.</userdoc>
        [DataMember(7)]
        public float SceneUnit { get; set; }

        /// <summary>
        /// Gets or sets the camera.
        /// </summary>
        /// <value>The camera.</value>
        /// <userdoc>The settings for the camera used in the scene editor</userdoc>
        [DataMember(10)]
        [NotNull]
        [Display("Camera", Expand = ExpandRule.Always)]
        public SceneEditorCameraSettings Camera { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether to use HDR when displaying a scene in the editor.
        /// </summary>
        /// <value><c>true</c> if [use HDR]; otherwise, <c>false</c>.</value>
        /// <userdoc>Specifies the type of rendering used in the scene editor. Basically, specifies if the scene editor should run in LDR or HDR mode.</userdoc>
        [DataMember(20)]
        [NotNull]
        [Display("Rendering Mode", Expand = ExpandRule.Always)]
        public ISceneEditorGraphicsModeSettings Mode { get; set; }
    }
}