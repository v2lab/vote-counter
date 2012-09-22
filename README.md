# Vote Counter for live democracy by The People Speak

This is an app to help [The People Speak][1] to count votes during their live democracy events.

The app is an image viewer with point-and-click interface for training card colors and counting cards in incoming images.

## The algorithms

### Training

User selects a color that needs training and clicks on several examples on the currently loaded snapshot. [Flood fill][5] algorithm is used to understand what she means. For a time being the filled contour is marked on the invisible `train.contours.COLOR` map, is shown as a white outline and counted.

Once enough cards of the color are pointed, user selects a different color and repeats the procedure.

When all colors are sampled user clicks "learn colors" button and the app collects all the selected pixels in bunches per color and performs [K-means clustering][2] on each bunch. Number of clusters (i.e. color gradations) is hardcoded (currently to 5). Learned colors are displayed for human inspection.

The app would then construct a [K Nearest Neighbors][3] classifier using learned color gradations as features.

NB: clustering and KNN are performed in [CIELab color space][4] using Euclidean distance as dissimilarity measure.

### Counting

When counting, all pixels of the incoming picture (converted to CIE Lab color space) are classified using K Nearest Neighbors search with K=1. The algorithm builds two maps: indices of the most-similar color per pixel and dissimilarities between pixel color and chosen palette color. The dissimilarity image is then thresholded on a value that user can interactively adjust. While finetuning the threshold value user sees the result of the thresholding as a posterized version of the input image with the pixels too dissimilar to one of the learned card colors painted black. After thresholding the dissimilarity map is split into three, one for each card color. Contiguous contours are searched in each of them and are shown as white outlines on top of the original image. Not all contours are shown / counted though - additional contour-area filter selects only blobs that are larger than a second interactively found threshold.

### Manual correction

The counter would still make some mistakes, which can be corrected manually by either *picking* (clicking with a left mouse button) to select a filtered out card or *unpicking* (clicking with a right mouse button) to deselect an area of the card color which isn't a card (or often a card that participant forgot to hide).

[1]: http://thepeoplespeak.org.uk/
[2]: http://en.wikipedia.org/wiki/K-means_clustering
[3]: http://en.wikipedia.org/wiki/K-nearest_neighbor_algorithm
[4]: http://en.wikipedia.org/wiki/Lab_color_space#CIELAB
[5]: http://en.wikipedia.org/wiki/Flood_fill
